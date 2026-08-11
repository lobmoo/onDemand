"""
OnDemand DDS Monitor - Layer 3: TUI 交互终端

用 rich 做渲染 (表格、面板、进度条、颜色)，用 curses 做键盘交互。
支持 F1-F4 视图切换、Q 退出、R 重置、P 暂停。
"""

import curses
import logging
import threading
import time
from dataclasses import dataclass

from rich.console import Console
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

try:
    from .config import MonitorConfig
    from .metrics import MetricsEngine, StatsSnapshot
except ImportError:
    from config import MonitorConfig
    from metrics import MetricsEngine, StatsSnapshot

logger = logging.getLogger(__name__)


@dataclass
class TUIState:
    """TUI 状态"""
    current_view: int = 1  # F1-F4
    paused: bool = False
    scroll_offset: int = 0
    running: bool = True


class MonitorTUI:
    """交互终端 TUI"""

    def __init__(self, config: MonitorConfig, metrics: MetricsEngine):
        self.config = config
        self.metrics = metrics
        self.state = TUIState()
        self._console = Console()
        self._thread: threading.Thread | None = None

    def start(self):
        """启动 TUI 主循环"""
        self._thread = threading.Thread(target=self._run, daemon=True, name="tui-main")
        self._thread.start()

    def stop(self):
        """停止 TUI"""
        self.state.running = False
        if self._thread:
            self._thread.join(timeout=3)

    def _run(self):
        """TUI 主循环 (curses)"""
        try:
            curses.wrapper(self._main_loop)
        except Exception as e:
            logger.error("TUI error: %s", e)

    def _main_loop(self, stdscr):
        """curses 主循环"""
        curses.curs_set(0)  # 隐藏光标
        stdscr.nodelay(True)  # 非阻塞输入
        stdscr.timeout(500)  # 500ms 超时

        # 定义颜色
        curses.start_color()
        curses.init_pair(1, curses.COLOR_GREEN, curses.COLOR_BLACK)  # OK
        curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)  # WARN
        curses.init_pair(3, curses.COLOR_RED, curses.COLOR_BLACK)  # CRIT
        curses.init_pair(4, curses.COLOR_CYAN, curses.COLOR_BLACK)  # INFO
        curses.init_pair(5, curses.COLOR_WHITE, curses.COLOR_BLUE)  # HEADER
        curses.init_pair(6, curses.COLOR_BLACK, curses.COLOR_WHITE)  # TAB

        while self.state.running:
            # 处理键盘输入
            self._handle_input(stdscr)

            # 渲染界面
            if not self.state.paused:
                snapshot = self.metrics.snapshot()
                self._render(stdscr, snapshot)

            time.sleep(1.0 / self.config.refresh_hz)

    def _handle_input(self, stdscr):
        """处理键盘输入"""
        try:
            key = stdscr.getch()
        except Exception:
            return

        if key == ord("q") or key == ord("Q"):
            self.state.running = False
        elif key == ord("r") or key == ord("R"):
            self.metrics.reset()
            self.state.scroll_offset = 0
        elif key == ord("p") or key == ord("P"):
            self.state.paused = not self.state.paused
        elif key == curses.KEY_F1:
            self.state.current_view = 1
            self.state.scroll_offset = 0
        elif key == curses.KEY_F2:
            self.state.current_view = 2
            self.state.scroll_offset = 0
        elif key == curses.KEY_F3:
            self.state.current_view = 3
            self.state.scroll_offset = 0
        elif key == curses.KEY_F4:
            self.state.current_view = 4
            self.state.scroll_offset = 0
        elif key == curses.KEY_UP:
            self.state.scroll_offset = max(0, self.state.scroll_offset - 1)
        elif key == curses.KEY_DOWN:
            self.state.scroll_offset += 1

    def _render(self, stdscr, snap: StatsSnapshot):
        """渲染当前视图"""
        stdscr.erase()
        max_y, max_x = stdscr.getmaxyx()

        # 顶部标题栏
        self._render_header(stdscr, max_x)

        # Tab 栏
        self._render_tabs(stdscr, max_x)

        # 内容区域
        content_y = 4
        content_h = max_y - 6  # 留出底部状态栏

        if self.state.current_view == 1:
            self._render_network_view(stdscr, snap, content_y, content_h, max_x)
        elif self.state.current_view == 2:
            self._render_topics_view(stdscr, snap, content_y, content_h, max_x)
        elif self.state.current_view == 3:
            self._render_variables_view(stdscr, snap, content_y, content_h, max_x)
        elif self.state.current_view == 4:
            self._render_performance_view(stdscr, snap, content_y, content_h, max_x)

        # 底部状态栏
        self._render_footer(stdscr, snap, max_y, max_x)

        stdscr.refresh()

    def _render_header(self, stdscr, max_x):
        """渲染顶部标题"""
        title = " OnDemand DDS Monitor "
        x = max(0, (max_x - len(title)) // 2)
        try:
            stdscr.addstr(0, x, title, curses.color_pair(5) | curses.A_BOLD)
        except curses.error:
            pass

    def _render_tabs(self, stdscr, max_x):
        """渲染 Tab 栏"""
        tabs = [
            (1, "F1: Network"),
            (2, "F2: Topics"),
            (3, "F3: Variables"),
            (4, "F4: Performance"),
        ]
        x = 2
        for view_id, label in tabs:
            if view_id == self.state.current_view:
                attr = curses.color_pair(6) | curses.A_BOLD
            else:
                attr = curses.color_pair(4)
            try:
                stdscr.addstr(2, x, f" {label} ", attr)
            except curses.error:
                pass
            x += len(label) + 4

    def _render_network_view(self, stdscr, snap: StatsSnapshot, y: int, h: int, max_x: int):
        """F1: 网络视图"""
        lines = []

        # Network Overview
        uptime_str = time.strftime("%H:%M:%S", time.gmtime(snap.uptime))
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Network Overview", curses.A_BOLD))
        lines.append((f"  Domain: {self.config.domain_id}    Interface: {self.config.interface}", 0))
        lines.append((f"  Uptime: {uptime_str}", 0))
        lines.append(("", 0))
        lines.append((f"  Total Packets:  {snap.pkt_count:>12,}", 0))
        lines.append((f"  Packet Rate:    {snap.pkt_rate:>12,.0f} pkt/s", 0))
        lines.append((f"  Total Bytes:    {snap.byte_count:>12,} B", 0))
        lines.append((f"  Throughput:     {snap.throughput:>12,.0f} B/s", 0))
        lines.append(("", 0))

        # RTPS Protocol Stats
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" RTPS Protocol Stats", curses.A_BOLD))
        lines.append((f"  HEARTBEAT:      {snap.hb_count:>12,}", 0))
        lines.append((f"  ACKNACK:        {snap.ack_count:>12,}", 0))
        lines.append((f"  DATA:           {snap.data_count:>12,}", 0))
        lines.append((f"  GAP:            {snap.gap_count:>12,}", 0))
        lines.append(("", 0))

        # Health indicators
        hb_color = self._health_color(snap.hb_loss_rate, self.config.hb_loss_warn, self.config.hb_loss_crit)
        resend_color = self._health_color(snap.resend_rate, self.config.resend_warn, self.config.resend_crit)
        lines.append((f"  HB Loss Rate:   {snap.hb_loss_rate*100:>11.4f}%", hb_color))
        lines.append((f"  Resend Rate:    {snap.resend_rate*100:>11.4f}%", resend_color))
        lines.append(("", 0))

        # Topic Distribution
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Topic Distribution", curses.A_BOLD))
        if snap.topic_stats:
            sorted_topics = sorted(snap.topic_stats.items(), key=lambda x: x[1]["pkts"], reverse=True)
            total_pkts = max(1, snap.pkt_count)
            for topic, stats in sorted_topics[:15]:
                pct = stats["pkts"] / total_pkts * 100
                # 缩短 topic 名称
                short_name = topic.split("/")[-1] if "/" in topic else topic
                lines.append(
                    (f"  {short_name:<25s} {stats['pkts']:>10,} pkts  {pct:>5.1f}%  {stats['rate']:>8.0f}/s", 0)
                )
        else:
            lines.append(("  (no data yet)", curses.A_DIM))

        # 渲染
        self._render_lines(stdscr, lines, y, h, max_x)

    def _render_topics_view(self, stdscr, snap: StatsSnapshot, y: int, h: int, max_x: int):
        """F2: Topics 视图"""
        lines = []

        # TableDefine 历史
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" TableDefine (pub → sub)", curses.A_BOLD))
        history = self.metrics._table_define_history
        if history:
            lines.append((f"  {'Time':<12s} {'Pub Node':<15s} {'Table':<20s} {'Vars':>8s}", curses.A_UNDERLINE))
            for entry in history[-20:]:
                t = time.strftime("%H:%M:%S", time.localtime(entry["time"]))
                lines.append(
                    (f"  {t:<12s} {entry['node']:<15s} {entry['table']:<20s} {entry['var_count']:>8,}", 0)
                )
        else:
            lines.append(("  (no tableDefine received yet)", curses.A_DIM))

        lines.append(("", 0))

        # SubTableRegister 历史
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" SubTableRegister (sub → pub)", curses.A_BOLD))
        sub_details = self.metrics._sub_details
        if sub_details:
            lines.append((f"  {'Time':<12s} {'Sub Node':<15s} {'Table':<20s} {'Vars':>8s}", curses.A_UNDERLINE))
            for entry in sub_details[-20:]:
                t = time.strftime("%H:%M:%S", time.localtime(entry["time"]))
                lines.append(
                    (f"  {t:<12s} {entry['node']:<15s} {entry['table']:<20s} {entry['var_count']:>8,}", 0)
                )
        else:
            lines.append(("  (no subRegister received yet)", curses.A_DIM))

        self._render_lines(stdscr, lines, y, h, max_x)

    def _render_variables_view(self, stdscr, snap: StatsSnapshot, y: int, h: int, max_x: int):
        """F3: Variables 视图"""
        lines = []

        # Pub/Sub Nodes
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Nodes", curses.A_BOLD))
        pub_str = ", ".join(snap.pub_nodes) if snap.pub_nodes else "(none)"
        sub_str = ", ".join(snap.sub_nodes) if snap.sub_nodes else "(none)"
        lines.append((f"  Pub Nodes: {pub_str}", curses.color_pair(1) if snap.pub_nodes else curses.A_DIM))
        lines.append((f"  Sub Nodes: {sub_str}", curses.color_pair(1) if snap.sub_nodes else curses.A_DIM))
        lines.append((f"  Total Variables: {snap.var_count:,}", curses.A_BOLD))
        lines.append(("", 0))

        # Bucket Distribution
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Bucket Distribution", curses.A_BOLD))
        lines.append((f"  {'Bucket':<8s} {'Vars':>8s} {'Pct':>6s}  {'Bar'}", curses.A_UNDERLINE))
        total_vars = max(1, snap.var_count)
        max_bucket = max(snap.bucket_var_count) if snap.bucket_var_count else 1
        for i in range(self.config.bucket_count):
            count = snap.bucket_var_count[i]
            pct = count / total_vars * 100 if total_vars > 0 else 0
            bar_len = int(count / max(max_bucket, 1) * 20) if max_bucket > 0 else 0
            bar = "█" * bar_len
            lines.append((f"  {i:<8d} {count:>8,} {pct:>5.1f}%  {bar}", 0))

        lines.append(("", 0))

        # Subscription Frequency Distribution
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Subscription Frequency Distribution", curses.A_BOLD))
        if snap.sub_freq_dist:
            for freq_ms, count in sorted(snap.sub_freq_dist.items()):
                if freq_ms >= 1000:
                    label = f"{freq_ms/1000:.1f}s"
                else:
                    label = f"{freq_ms}ms"
                lines.append((f"  {label:<10s}: {count:>6,} vars", 0))
        else:
            lines.append(("  (no subscriptions yet)", curses.A_DIM))

        self._render_lines(stdscr, lines, y, h, max_x)

    def _render_performance_view(self, stdscr, snap: StatsSnapshot, y: int, h: int, max_x: int):
        """F4: Performance 视图"""
        lines = []

        # Jitter
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Jitter (ms) per Bucket", curses.A_BOLD))
        lines.append((f"  {'Bucket':<8s} {'Jitter':>10s} {'Status':>8s}", curses.A_UNDERLINE))
        for i in range(self.config.bucket_count):
            jitter = snap.jitter_ms[i]
            color = self._health_color(jitter / 1000, self.config.jitter_warn_ms / 1000, self.config.jitter_crit_ms / 1000)
            status = "OK" if jitter < self.config.jitter_warn_ms else ("WARN" if jitter < self.config.jitter_crit_ms else "CRIT")
            lines.append((f"  {i:<8d} {jitter:>10.3f} {status:>8s}", color))

        lines.append(("", 0))

        # Throughput
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Throughput (bytes) per Bucket", curses.A_BOLD))
        lines.append((f"  {'Bucket':<8s} {'Bytes':>12s} {'MsgRate':>10s} {'AvgVars':>8s}", curses.A_UNDERLINE))
        total_throughput = 0
        for i in range(self.config.bucket_count):
            tp = snap.throughput_per_bucket[i]
            total_throughput += tp
            rate = snap.data_msg_rate[i]
            avg_vars = snap.mask_var_count_avg[i]
            lines.append((f"  {i:<8d} {tp:>12,.0f} {rate:>10.1f}/s {avg_vars:>8.1f}", 0))
        lines.append(("", 0))
        lines.append((f"  TOTAL:  {total_throughput:>12,.0f} bytes", curses.A_BOLD))

        lines.append(("", 0))

        # Protocol Health Summary
        lines.append(("─" * (max_x - 4), curses.A_DIM))
        lines.append((" Protocol Health", curses.A_BOLD))
        avg_jitter = sum(snap.jitter_ms) / max(1, len([j for j in snap.jitter_ms if j > 0]))
        hb_color = self._health_color(snap.hb_loss_rate, self.config.hb_loss_warn, self.config.hb_loss_crit)
        resend_color = self._health_color(snap.resend_rate, self.config.resend_warn, self.config.resend_crit)
        jitter_color = self._health_color(avg_jitter / 1000, self.config.jitter_warn_ms / 1000, self.config.jitter_crit_ms / 1000)

        lines.append((f"  HB Loss Rate:   {snap.hb_loss_rate*100:>10.4f}%  {self._health_label(snap.hb_loss_rate, self.config.hb_loss_warn, self.config.hb_loss_crit)}", hb_color))
        lines.append((f"  Resend Rate:    {snap.resend_rate*100:>10.4f}%  {self._health_label(snap.resend_rate, self.config.resend_warn, self.config.resend_crit)}", resend_color))
        lines.append((f"  Avg Jitter:     {avg_jitter:>10.3f}ms  {self._health_label(avg_jitter/1000, self.config.jitter_warn_ms/1000, self.config.jitter_crit_ms/1000)}", jitter_color))

        self._render_lines(stdscr, lines, y, h, max_x)

    def _render_footer(self, stdscr, snap: StatsSnapshot, max_y: int, max_x: int):
        """渲染底部状态栏"""
        status = "PAUSED" if self.state.paused else "Running"
        status_color = curses.color_pair(2) if self.state.paused else curses.color_pair(1)
        footer = f" [{status}] Domain:{self.config.domain_id} | {self.config.interface} | Q:Quit R:Reset P:Pause"
        try:
            stdscr.addstr(max_y - 1, 0, footer[:max_x - 1], status_color)
        except curses.error:
            pass

    def _render_lines(self, stdscr, lines: list, y: int, h: int, max_x: int):
        """渲染文本行列表"""
        start = self.state.scroll_offset
        for i, (text, attr) in enumerate(lines[start:]):
            if i >= h:
                break
            try:
                stdscr.addnstr(y + i, 2, text, max_x - 4, attr)
            except curses.error:
                pass

    def _health_color(self, value: float, warn: float, crit: float) -> int:
        """根据阈值返回颜色"""
        if value >= crit:
            return curses.color_pair(3)  # RED
        elif value >= warn:
            return curses.color_pair(2)  # YELLOW
        return curses.color_pair(1)  # GREEN

    def _health_label(self, value: float, warn: float, crit: float) -> str:
        """根据阈值返回状态标签"""
        if value >= crit:
            return "CRIT"
        elif value >= warn:
            return "WARN"
        return "OK"
