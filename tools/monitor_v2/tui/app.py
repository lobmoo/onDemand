"""OnDemand DDS Monitor v2.0 - Textual App"""
import os
import time
from typing import Optional

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal, Vertical
from textual.reactive import reactive
from textual.screen import Screen
from textual.widgets import (
    DataTable, Footer, Header, Label, ListItem, ListView,
    Static, TabbedContent, TabPane, ProgressBar,
)

from core.metrics import MetricsEngine
from core.pcap import PcapWorker
from config import MonitorConfig


def fmt_bytes(n: float) -> str:
    for u in ["B", "KB", "MB", "GB"]:
        if n < 1024:
            return f"{n:.1f}{u}"
        n /= 1024
    return f"{n:.1f}TB"


def fmt_rate(n: float) -> str:
    if n >= 1e6:
        return f"{n/1e6:.1f}M"
    if n >= 1e3:
        return f"{n/1e3:.1f}K"
    return f"{n:.0f}"


def fmt_freq(rate: float) -> str:
    if rate <= 0:
        return "--"
    interval = 1.0 / rate
    if interval < 0.001:
        return f"{interval*1e6:.0f}us"
    elif interval < 1.0:
        return f"{interval*1e3:.0f}ms"
    return f"{interval:.1f}s"


def health_label(value: float, warn: float, crit: float) -> str:
    if value >= crit:
        return "[CRIT]"
    elif value >= warn:
        return "[WARN]"
    return "[OK]"


def health_class(value: float, warn: float, crit: float) -> str:
    if value >= crit:
        return "crit"
    elif value >= warn:
        return "warn"
    return "ok"


# ── Page 0: Network Discovery ──

class DiscoveryPage(Static):
    """Page 0: Domain discovery + raw packet feed"""

    def __init__(self, metrics: MetricsEngine, config: MonitorConfig, **kwargs):
        super().__init__(**kwargs)
        self.metrics = metrics
        self.config = config

    def compose(self) -> ComposeResult:
        yield Label("Network Discovery & Domain Scanner", id="p0-title")
        yield DataTable(id="domain-table")
        yield Label("", id="p0-stats")
        yield DataTable(id="packet-table")

    def on_mount(self):
        # Domain table
        dt = self.query_one("#domain-table", DataTable)
        dt.add_columns("Domain ID", "Participants", "Ports", "Packets", "Status")
        dt.cursor_type = "row"

        # Packet table
        pt = self.query_one("#packet-table", DataTable)
        pt.add_columns("Time", "Src", "Dst", "Domain", "Type", "Topic", "Size")

    def refresh_display(self):
        snap = self.metrics.snapshot()

        # Update domain table
        dt = self.query_one("#domain-table", DataTable)
        dt.clear()
        for d in snap["domain_list"]:
            did = d["domain_id"]
            parts = d["participants"]
            ports_str = ", ".join(str(p) for p in sorted(d["ports"].keys()))
            pkts = d["pkt_count"]
            status = "Active" if time.time() - d["last_seen"] < 5 else "Stale"
            dt.add_row(str(did), str(parts), ports_str, f"{pkts:,}", status)

        # Update stats
        up = snap["uptime"]
        hb = snap["submsg_counts"].get(0x07, 0)
        ack = snap["submsg_counts"].get(0x06, 0)
        gap = snap["submsg_counts"].get(0x08, 0)
        hb_loss = max(0, 1.0 - ack / hb) if hb > 0 else 0
        stats_text = (
            f"Uptime: {up:.0f}s  |  "
            f"Packets: {snap['pkt_total']:,}  RTPS: {snap['pkt_rtps']:,}  |  "
            f"Rate: {fmt_rate(snap['pkt_rate'])}/s  {fmt_bytes(snap['byte_rate'])}/s  |  "
            f"HB Loss: {hb_loss*100:.2f}% {health_label(hb_loss, 0.01, 0.05)}  |  "
            f"GAP: {gap:,}  |  "
            f"Domains: {len(snap['domain_list'])}"
        )
        self.query_one("#p0-stats", Label).update(stats_text)

        # Update packet table (last 50)
        pt = self.query_one("#packet-table", DataTable)
        pt.clear()
        pkts = snap["rtps_packets"][-50:]
        for p in pkts:
            t = time.strftime("%H:%M:%S", time.localtime(p["time"]))
            pt.add_row(
                t, str(p["sport"]), str(p["dport"]),
                str(p["domain"]), p["type"],
                p["topic"][:40] if p["topic"] else "-",
                str(p["size"]),
            )


# ── Page 1: Topic Monitor ──

class TopicPage(Static):
    """Page 1: Topic discovery + protocol diagnostics"""

    def __init__(self, metrics: MetricsEngine, config: MonitorConfig, **kwargs):
        super().__init__(**kwargs)
        self.metrics = metrics
        self.config = config
        self._selected_topic: Optional[str] = None
        self._detail_mode = False

    def compose(self) -> ComposeResult:
        yield Label("Topic Monitor", id="p1-title")
        yield DataTable(id="topic-table")
        yield Label("", id="p1-detail-header")
        yield DataTable(id="detail-table")
        yield Label("", id="p1-events")

    def on_mount(self):
        tt = self.query_one("#topic-table", DataTable)
        tt.add_columns("Topic", "Type", "Writer", "Pkts", "Bytes", "Rate", "Loss%", "GAP", "FRAG")
        tt.cursor_type = "row"

        dt = self.query_one("#detail-table", DataTable)
        dt.add_columns("Metric", "Value", "Status")

    def on_data_table_row_selected(self, event: DataTable.RowSelected):
        if event.data_table.id == "topic-table":
            snap = self.metrics.snapshot()
            topics = snap["topic_list"]
            if event.row_key and event.row_key.value is not None:
                idx = int(str(event.row_key.value))
                if idx < len(topics):
                    self._selected_topic = topics[idx]["name"]
                    self._detail_mode = True

    def go_back(self):
        self._detail_mode = False
        self._selected_topic = None

    def refresh_display(self):
        snap = self.metrics.snapshot()

        if self._detail_mode and self._selected_topic:
            self._refresh_detail(snap)
        else:
            self._refresh_topic_list(snap)

        # Events
        events_text = "  ".join(snap["events"][-5:]) if snap["events"] else ""
        self.query_one("#p1-events", Label).update(events_text)

    def _refresh_topic_list(self, snap):
        self.query_one("#p1-title", Label).update(
            f"Topic Monitor  (Domain: {snap['domain_list'][0]['domain_id'] if snap['domain_list'] else '?'})"
        )
        self.query_one("#p1-detail-header", Label).update("")
        tt = self.query_one("#topic-table", DataTable)
        tt.clear()
        tt.display = True
        self.query_one("#detail-table", DataTable).display = False

        for i, t in enumerate(snap["topic_list"]):
            tt.add_row(
                t["name"][:50], t["type_name"][:30], t["writer_id"],
                f"{t['pkt_count']:,}", fmt_bytes(t["byte_count"]),
                f"{fmt_rate(t['rate'])}/s",
                f"{t['loss_rate']*100:.3f}",
                f"{t['gap_count']:,}", f"{t['frag_count']:,}",
                key=str(i),
            )

    def _refresh_detail(self, snap):
        topic = self._selected_topic
        self.query_one("#p1-title", Label).update(f"Topic Detail: {topic}")
        self.query_one("#p1-detail-header", Label).update("Press ESC to go back")

        self.query_one("#topic-table", DataTable).display = False
        dt = self.query_one("#detail-table", DataTable)
        dt.display = True
        dt.clear()

        # Find topic info
        tinfo = None
        for t in snap["topic_list"]:
            if t["name"] == topic:
                tinfo = t
                break
        if not tinfo:
            return

        rate = tinfo["rate"]
        loss = tinfo["loss_rate"]
        config = self.config

        rows = [
            ("Topic Name", tinfo["name"], ""),
            ("Type", tinfo["type_name"], ""),
            ("Writer ID", tinfo["writer_id"], ""),
            ("", "", ""),
            ("Packets", f"{tinfo['pkt_count']:,}", ""),
            ("Bytes", fmt_bytes(tinfo["byte_count"]), ""),
            ("Rate", f"{fmt_rate(rate)}/s ({fmt_freq(rate)})", ""),
            ("Avg Size", f"{tinfo['avg_size']:.0f} bytes", ""),
            ("", "", ""),
            ("GAP (retransmit)", f"{tinfo['gap_count']:,}", ""),
            ("DATA_FRAG", f"{tinfo['frag_count']:,}", ""),
            ("Lost (SN)", f"{tinfo['lost_count']:,}", ""),
            ("Received (SN)", f"{tinfo['received_count']:,}", ""),
            ("Loss Rate", f"{loss*100:.4f}%", health_label(loss, config.loss_warn, config.loss_crit)),
        ]
        for metric, value, status in rows:
            dt.add_row(metric, value, status)


# ── Page 2: Variable Monitor ──

class VariablePage(Static):
    """Page 2: OnDemand variable-level monitoring"""

    def __init__(self, metrics: MetricsEngine, config: MonitorConfig, **kwargs):
        super().__init__(**kwargs)
        self.metrics = metrics
        self.config = config
        self._filter = ""
        self._detail_mode = False
        self._selected_var_id: Optional[int] = None

    def compose(self) -> ComposeResult:
        yield Label("Variable Monitor", id="p2-title")
        yield Label("", id="p2-summary")
        yield DataTable(id="var-table")
        yield DataTable(id="sub-table")
        yield Label("", id="p2-events")

    def on_mount(self):
        vt = self.query_one("#var-table", DataTable)
        vt.add_columns("VarHash", "Name", "Node", "Bucket", "Size", "Table")
        vt.cursor_type = "row"

        st = self.query_one("#sub-table", DataTable)
        st.add_columns("Node", "Table", "Type", "VarCount", "Vars(Preview)")

    def set_filter(self, f: str):
        self._filter = f.lower()

    def refresh_display(self):
        snap = self.metrics.snapshot()

        # Summary
        total = snap["total_vars"]
        pubs = len(snap["pub_table_defines"])
        subs = len(snap["known_subs"])
        bv = snap["bucket_var_count"]
        active_buckets = sum(1 for c in bv if c > 0)
        self.query_one("#p2-summary", Label).update(
            f"Variables: {total}  |  PubTableDefine msgs: {pubs}  |  "
            f"Sub nodes: {subs}  |  Active buckets: {active_buckets}/20"
        )

        # Title
        self.query_one("#p2-title", Label).update(
            f"Variable Monitor  (filter: '{self._filter}')" if self._filter else "Variable Monitor"
        )

        # Variable table
        vt = self.query_one("#var-table", DataTable)
        vt.clear()
        for vid, v in sorted(snap["known_vars"].items()):
            name = v.get("name", "")
            if self._filter and self._filter not in name.lower():
                continue
            vt.add_row(
                f"0x{vid:016x}", name[:30], v.get("nodeName", "")[:15],
                str(v.get("bucket", 0)), str(v.get("size", 0)),
                v.get("table", "")[:20],
            )

        # Sub table
        st = self.query_one("#sub-table", DataTable)
        st.clear()
        for node, info in sorted(snap["known_subs"].items()):
            msg_type = "SUB" if info.get("msgType", 0) == 0 else "UNSUB"
            vars_list = info.get("vars", [])
            preview = ", ".join(v["name"][:15] for v in vars_list[:3])
            if len(vars_list) > 3:
                preview += f" +{len(vars_list)-3}"
            st.add_row(
                node[:20], info.get("tableName", "")[:15],
                msg_type, str(len(vars_list)), preview,
            )

        # Events
        events_text = "  ".join(snap["events"][-5:]) if snap["events"] else ""
        self.query_one("#p2-events", Label).update(events_text)


# ── Main Monitor App ──

class MonitorApp(App):
    """OnDemand DDS Monitor v2.0 - Textual Application"""

    CSS_PATH = "styles/monitor.tcss"
    TITLE = "OnDemand DDS Monitor v2.0"

    BINDINGS = [
        Binding("1", "show_page(0)", "Discovery"),
        Binding("2", "show_page(1)", "Topics"),
        Binding("3", "show_page(2)", "Variables"),
        Binding("left", "prev_page", "Prev"),
        Binding("right", "next_page", "Next"),
        Binding("escape", "go_back", "Back"),
        Binding("p", "toggle_pause", "Pause"),
        Binding("c", "clear_stats", "Clear"),
        Binding("q", "quit", "Quit"),
        Binding("slash", "focus_search", "Search", show=False),
    ]

    def __init__(self, config: MonitorConfig, metrics: MetricsEngine, pcap: PcapWorker, **kwargs):
        super().__init__(**kwargs)
        self.config = config
        self.metrics = metrics
        self.pcap = pcap
        self._current_page = 0
        self._paused = False

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal(id="nav-bar"):
            yield Label(" [0] Discovery ", id="tab-0", classes="nav-tab active")
            yield Label(" [1] Topics ", id="tab-1", classes="nav-tab")
            yield Label(" [2] Variables ", id="tab-2", classes="nav-tab")
        with TabbedContent(id="content"):
            with TabPane("Discovery", id="page-0"):
                yield DiscoveryPage(self.metrics, self.config, id="discovery-page")
            with TabPane("Topics", id="page-1"):
                yield TopicPage(self.metrics, self.config, id="topic-page")
            with TabPane("Variables", id="page-2"):
                yield VariablePage(self.metrics, self.config, id="variable-page")
        yield Label("", id="footer-bar")

    def on_mount(self):
        self.set_interval(1.0 / self.config.refresh_hz, self._tick)

    def _tick(self):
        if self._paused:
            return

        # Refresh current page
        if self._current_page == 0:
            self.query_one("#discovery-page", DiscoveryPage).refresh_display()
        elif self._current_page == 1:
            self.query_one("#topic-page", TopicPage).refresh_display()
        elif self._current_page == 2:
            self.query_one("#variable-page", VariablePage).refresh_display()

        # Footer
        snap = self.metrics.snapshot()
        pause_str = " [PAUSED]" if self._paused else ""
        self.query_one("#footer-bar", Label).update(
            f" iface={self.config.interface}  "
            f"domains={len(snap['domain_list'])}  "
            f"topics={len(snap['topic_list'])}  "
            f"vars={snap['total_vars']}{pause_str}  "
            f"[1/2/3]Pages [P]ause [C]lear [/]Search [Q]uit"
        )

    def _switch_page(self, idx: int):
        self._current_page = idx
        for i in range(3):
            tab = self.query_one(f"#tab-{i}", Label)
            if i == idx:
                tab.add_class("active")
            else:
                tab.remove_class("active")
        tc = self.query_one("#content", TabbedContent)
        tc.active = f"page-{idx}"

    def action_show_page(self, idx: int):
        self._switch_page(idx)

    def action_prev_page(self):
        self._switch_page(max(0, self._current_page - 1))

    def action_next_page(self):
        self._switch_page(min(2, self._current_page + 1))

    def action_go_back(self):
        if self._current_page == 1:
            tp = self.query_one("#topic-page", TopicPage)
            if tp._detail_mode:
                tp.go_back()
                return
        self.action_prev_page()

    def action_toggle_pause(self):
        self._paused = not self._paused

    def action_clear_stats(self):
        self.metrics.reset()

    def action_focus_search(self):
        if self._current_page == 2:
            # TODO: focus search input
            pass