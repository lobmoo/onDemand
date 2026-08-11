#!/usr/bin/env python3
"""
OnDemand DDS Monitor - 基于 writerId 映射的抓包监控
用法:
  sudo python3 run2.py -i enx0826ae3a20b7
  sudo python3 run2.py -i lo --debug  # 抓所有UDP
"""
import argparse
import curses
import logging
import socket
import struct
import threading
import time
from collections import defaultdict, deque

RTPS_MAGIC = b"RTPS"
SUBMSG_NAMES = {
    0x01: "PAD", 0x06: "ACKNACK", 0x07: "HEARTBEAT",
    0x08: "GAP", 0x09: "INFO_TS", 0x0e: "INFO_DST",
    0x15: "DATA", 0x16: "DATA_FRAG", 0x80: "VENDOR",
}

# ═══════════════════════════════════════════════════════════════
#  滑动窗口速率计
# ═══════════════════════════════════════════════════════════════
class RateMeter:
    def __init__(self, window=2.0):
        self._window = window
        self._events = deque()
        self._lock = threading.Lock()

    def add(self, ts=None, val=1.0):
        if ts is None:
            ts = time.time()
        with self._lock:
            self._events.append((ts, val))
            self._evict(ts)

    def _evict(self, now):
        cutoff = now - self._window
        while self._events and self._events[0][0] < cutoff:
            self._events.popleft()

    def rate(self):
        with self._lock:
            if len(self._events) < 2:
                return 0.0
            dt = self._events[-1][0] - self._events[0][0]
            return (len(self._events) - 1) / dt if dt > 0 else 0.0

    def count(self):
        with self._lock:
            return len(self._events)


# ═══════════════════════════════════════════════════════════════
#  统计引擎
# ═══════════════════════════════════════════════════════════════
class Stats:
    def __init__(self):
        self.lock = threading.Lock()
        self.t0 = time.time()

        # 包级别
        self.pkt_total = 0
        self.pkt_rtps = 0
        self.bytes_total = 0

        # 子消息计数
        self.submsg_counts = defaultdict(int)

        # DATA writer 统计: wid_hex -> {count, bytes, rate}
        self.data_writers = defaultdict(lambda: {"count": 0, "bytes": 0, "rate": RateMeter()})

        # SPDP/SEDP writer 统计
        self.spdp_count = 0
        self.sedp_pub_count = 0
        self.sedp_sub_count = 0

        # 端口统计
        self.port_counts = defaultdict(int)

        # 速率
        self.pkt_rate = RateMeter()
        self.byte_rate = RateMeter()
        self.data_rate = RateMeter()

        # 最近事件
        self.events = deque(maxlen=30)

    def on_packet(self, pkt_len, sport=0, dport=0):
        now = time.time()
        with self.lock:
            self.pkt_total += 1
            self.bytes_total += pkt_len
            self.pkt_rate.add(now)
            self.byte_rate.add(now, pkt_len)
            if dport in (7400, 7401):
                self.port_counts[dport] += 1
            elif sport in (7400, 7401):
                self.port_counts[sport] += 1

    def on_rtps(self):
        with self.lock:
            self.pkt_rtps += 1

    def on_submsg(self, sid, wid_hex=None, frag_len=0, port=0):
        now = time.time()
        with self.lock:
            self.submsg_counts[sid] += 1

            if sid == 0x15:  # DATA
                self.data_rate.add(now)
                if wid_hex:
                    w = self.data_writers[wid_hex]
                    w["count"] += 1
                    w["bytes"] += frag_len
                    w["rate"].add(now)
                    # 识别 SPDP vs SEDP
                    if wid_hex == "000100c2":
                        self.spdp_count += 1
                    elif wid_hex == "000003c2":
                        self.sedp_pub_count += 1
                    elif wid_hex == "000004c2":
                        self.sedp_sub_count += 1

            elif sid == 0x16:  # DATA_FRAG
                self.data_rate.add(now)
                if wid_hex:
                    w = self.data_writers[wid_hex]
                    w["count"] += 1
                    w["bytes"] += frag_len
                    w["rate"].add(now)

    def snapshot(self):
        with self.lock:
            now = time.time()
            uptime = now - self.t0

            # 按 writer 分组: 区分 built-in (0xc2/0xc7) vs user (0x03)
            builtin_writers = {}
            user_writers = {}
            for wid, info in self.data_writers.items():
                entry = {
                    "count": info["count"],
                    "bytes": info["bytes"],
                    "rate": info["rate"].rate(),
                }
                kind = int(wid[-2:], 16)
                if kind == 0xc2:
                    builtin_writers[wid] = entry
                elif kind == 0x03:
                    user_writers[wid] = entry

            # user writers 排序 (by entity key)
            sorted_user = sorted(user_writers.items(), key=lambda x: int(x[0][:6], 16))

            return {
                "uptime": uptime,
                "pkt_total": self.pkt_total,
                "pkt_rtps": self.pkt_rtps,
                "bytes_total": self.bytes_total,
                "pkt_rate": self.pkt_rate.rate(),
                "byte_rate": self.byte_rate.rate(),
                "data_rate": self.data_rate.rate(),
                "submsg_counts": dict(self.submsg_counts),
                "sorted_user": sorted_user,
                "builtin_writers": builtin_writers,
                "spdp_count": self.spdp_count,
                "sedp_pub_count": self.sedp_pub_count,
                "sedp_sub_count": self.sedp_sub_count,
                "port_counts": dict(self.port_counts),
            }


# ═══════════════════════════════════════════════════════════════
#  抓包线程 (原生 socket)
# ═══════════════════════════════════════════════════════════════
class PcapWorker:
    def __init__(self, iface, stats, port_filter=None):
        self.iface = iface
        self.stats = stats
        self.port_filter = port_filter
        self.running = threading.Event()
        self.sock = None

    def start(self):
        self.running.set()
        threading.Thread(target=self._run, daemon=True, name="pcap").start()

    def stop(self):
        self.running.clear()
        if self.sock:
            try: self.sock.close()
            except: pass

    def _run(self):
        try:
            self.sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(0x0003))
            self.sock.bind((self.iface, 0))
            self.sock.settimeout(1.0)
        except Exception as e:
            logging.error("socket error: %s", e)
            return

        while self.running.is_set():
            try:
                data, _ = self.sock.recvfrom(65535)
                self._process(data)
            except socket.timeout:
                continue
            except Exception:
                pass

    def _process(self, data):
        if len(data) < 42:  # ETH(14) + IP(20) + UDP(8)
            return

        # Parse Ethernet -> IPv4 -> UDP
        etype = struct.unpack("!H", data[12:14])[0]
        if etype != 0x0800:
            return
        ihl = (data[14] & 0x0F) * 4
        if data[23] != 17:  # UDP
            return
        udp_start = 14 + ihl
        if len(data) < udp_start + 8:
            return

        sport, dport = struct.unpack("!HH", data[udp_start:udp_start+4])
        udp_len = struct.unpack("!H", data[udp_start+4:udp_start+6])[0]
        payload = data[udp_start+8:udp_start+udp_len]

        # 端口过滤
        if self.port_filter:
            if sport not in self.port_filter and dport not in self.port_filter:
                return

        pkt_len = len(payload)
        self.stats.on_packet(pkt_len, sport, dport)

        # RTPS check
        if pkt_len < 20 or payload[:4] != RTPS_MAGIC:
            return
        self.stats.on_rtps()

        # Parse submessages
        offset = 20
        while offset + 4 <= len(payload):
            sid = payload[offset]
            flags = payload[offset + 1]
            is_be = not (flags & 0x01)
            fmt = "!H" if is_be else "<H"
            length = struct.unpack_from(fmt, payload, offset + 2)[0]
            if length == 0:
                break

            if sid in (0x15, 0x16) and length >= 16:  # DATA / DATA_FRAG
                body = offset + 4
                wid = payload[body+8:body+12].hex()
                port = dport if dport in (7400, 7401) else sport
                self.stats.on_submsg(sid, wid_hex=wid, frag_len=length, port=port)
            else:
                self.stats.on_submsg(sid)

            offset += 4 + length


# ═══════════════════════════════════════════════════════════════
#  TUI
# ═══════════════════════════════════════════════════════════════
C_GRN = 1; C_YEL = 2; C_RED = 3; C_CYA = 4; C_HDR = 5; C_DIM = 6; C_WHT = 7

def init_colors():
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(C_GRN, curses.COLOR_GREEN, -1)
    curses.init_pair(C_YEL, curses.COLOR_YELLOW, -1)
    curses.init_pair(C_RED, curses.COLOR_RED, -1)
    curses.init_pair(C_CYA, curses.COLOR_CYAN, -1)
    curses.init_pair(C_HDR, curses.COLOR_WHITE, curses.COLOR_BLUE)
    curses.init_pair(C_DIM, curses.COLOR_WHITE, -1)
    curses.init_pair(C_WHT, curses.COLOR_WHITE, -1)

def safe(s, y, x, text, attr=0, mx=None):
    my, m = s.getmaxyx()
    if mx: m = min(m, mx)
    if y < 0 or y >= my or x < 0: return
    avail = m - x
    if avail <= 0: return
    try: s.addnstr(y, x, text[:avail], avail, attr)
    except: pass

def fmt_b(n):
    for u in ["B", "KB", "MB", "GB"]:
        if n < 1024: return f"{n:.1f}{u}"
        n /= 1024
    return f"{n:.1f}TB"

def fmt_r(n):
    if n >= 1e6: return f"{n/1e6:.1f}M"
    if n >= 1e3: return f"{n/1e3:.1f}K"
    return f"{n:.0f}"

def hcolor(v, w, c):
    return curses.color_pair(C_RED if v >= c else C_YEL if v >= w else C_GRN)

def hlabel(v, w, c):
    return "CRIT" if v >= c else "WARN" if v >= w else "OK"

def sep(s, y, mx, label=""):
    if label:
        safe(s, y, 2, f"── {label} ", curses.color_pair(C_DIM), mx)
    else:
        safe(s, y, 2, "─" * (mx - 4), curses.color_pair(C_DIM))

VIEWS = ["Overview", "Writers", "Protocol"]


def draw_overview(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll
    up = time.strftime("%H:%M:%S", time.gmtime(snap["uptime"]))
    if y >= 3: safe(s, y, 2, f" Uptime: {up}    Packets: {snap['pkt_total']:>10,}    RTPS: {snap['pkt_rtps']:>10,}    Bytes: {fmt_b(snap['bytes_total']):>10s}", mx=mx)
    y += 1
    if y >= 3: safe(s, y, 2, f" Pkt Rate: {fmt_r(snap['pkt_rate'])}/s    Byte Rate: {fmt_b(snap['byte_rate'])}/s    DATA Rate: {fmt_r(snap['data_rate'])}/s", mx=mx)
    y += 2

    # Port stats
    if y >= 3: sep(s, y, mx, "Port Distribution")
    y += 1
    for port in sorted(snap.get("port_counts", {}).keys()):
        if y >= my - 2: break
        cnt = snap["port_counts"][port]
        label = "discovery" if port == 7400 else "userdata"
        if y >= 3: safe(s, y, 4, f"  Port {port} ({label}): {cnt:>10,} packets", mx=mx)
        y += 1
    y += 1

    # Submessage types
    if y >= 3: sep(s, y, mx, "RTPS Submessage Types")
    y += 1
    for sid in sorted(snap["submsg_counts"].keys(), key=lambda x: snap["submsg_counts"][x], reverse=True):
        if y >= my - 2: break
        cnt = snap["submsg_counts"][sid]
        name = SUBMSG_NAMES.get(sid, f"0x{sid:02x}")
        if y >= 3: safe(s, y, 4, f"  {name:<12s}: {cnt:>10,}", mx=mx)
        y += 1
    y += 1

    # Discovery stats
    if y >= 3: sep(s, y, mx, "Discovery (Built-in)")
    y += 1
    if y >= 3: safe(s, y, 4, f"  SPDP Participant: {snap['spdp_count']:>8,}", curses.color_pair(C_CYA), mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  SEDP Publications: {snap['sedp_pub_count']:>7,}", curses.color_pair(C_CYA), mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  SEDP Subscriptions: {snap['sedp_sub_count']:>6,}", curses.color_pair(C_CYA), mx=mx)


def draw_writers(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll

    # User writers (data plane)
    if y >= 3: sep(s, y, mx, "User Writers (Data Plane)")
    y += 1
    if y >= 3:
        safe(s, y, 4, f"{'WriterId':<10s} {'EntityKey':<10s} {'Pkts':>8s} {'Bytes':>10s} {'Rate':>8s}", curses.A_UNDERLINE, mx=mx)
    y += 1

    writers = snap.get("sorted_user", [])
    for wid, info in writers:
        if y >= my - 2: break
        if y < 3: y += 1; continue
        ekey = wid[:6]
        safe(s, y, 4, f"{wid:<10s} 0x{ekey:<8s} {info['count']:>8,} {fmt_b(info['bytes']):>10s} {fmt_r(info['rate'])+'/s':>8s}", mx=mx)
        y += 1

    y += 1
    # Summary
    total_data = sum(info["count"] for _, info in writers)
    total_bytes = sum(info["bytes"] for _, info in writers)
    total_rate = sum(info["rate"] for _, info in writers)
    if y >= 3: safe(s, y, 4, f"  Total: {len(writers)} writers, {total_data:,} packets, {fmt_b(total_bytes)}, {fmt_r(total_rate)}/s",
                     curses.color_pair(C_GRN), mx=mx)
    y += 2

    # Built-in writers
    if y >= 3: sep(s, y, mx, "Built-in Writers (Discovery)")
    y += 1
    for wid, info in snap.get("builtin_writers", {}).items():
        if y >= my - 2: break
        if y < 3: y += 1; continue
        label = "SPDP" if wid == "000100c2" else ("SEDP-Pub" if wid == "000003c2" else ("SEDP-Sub" if wid == "000004c2" else "Unknown"))
        safe(s, y, 4, f"  {wid} ({label}): {info['count']:>8,} pkts  {fmt_r(info['rate'])}/s",
             curses.color_pair(C_CYA), mx=mx)
        y += 1


def draw_protocol(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll

    # Heartbeat / ACK / GAP analysis
    hb = snap["submsg_counts"].get(0x07, 0)
    ack = snap["submsg_counts"].get(0x06, 0)
    gap = snap["submsg_counts"].get(0x08, 0)
    data = snap["submsg_counts"].get(0x15, 0)
    frag = snap["submsg_counts"].get(0x16, 0)
    total_data = data + frag

    if y >= 3: sep(s, y, mx, "Protocol Health")
    y += 1

    # HB loss rate
    hb_loss = max(0, 1.0 - ack / hb) if hb > 0 else 0
    hb_pct = hb_loss * 100
    if y >= 3:
        safe(s, y, 4, f"  HB Loss Rate:    {hb_pct:>10.4f}%  ", mx=mx)
        safe(s, y, 38, f"[{hlabel(hb_loss, 0.01, 0.05)}]", hcolor(hb_loss, 0.01, 0.05), mx=mx)
    y += 1

    # Resend rate
    resend = gap / total_data if total_data > 0 else 0
    resend_pct = resend * 100
    if y >= 3:
        safe(s, y, 4, f"  Resend Rate:     {resend_pct:>10.4f}%  ", mx=mx)
        safe(s, y, 38, f"[{hlabel(resend, 0.005, 0.02)}]", hcolor(resend, 0.005, 0.02), mx=mx)
    y += 2

    # Message rates
    if y >= 3: sep(s, y, mx, "Message Rates (avg)")
    y += 1
    up = max(1, snap["uptime"])
    rates = [
        ("HEARTBEAT", hb / up, C_CYA),
        ("ACKNACK", ack / up, C_YEL),
        ("DATA", data / up, C_GRN),
        ("DATA_FRAG", frag / up, C_GRN),
        ("GAP", gap / up, C_RED),
    ]
    for name, rate, color in rates:
        if y >= my - 2: break
        if y >= 3: safe(s, y, 4, f"  {name:<12s}: {fmt_r(rate)+'/s':>10s}", curses.color_pair(color), mx=mx)
        y += 1

    y += 1
    if y >= 3: sep(s, y, mx, "Throughput")
    y += 1
    if y >= 3: safe(s, y, 4, f"  Current:  {fmt_b(snap['byte_rate'])}/s", mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  Total:    {fmt_b(snap['bytes_total'])}", mx=mx)
    y += 1
    avg = snap["bytes_total"] / up
    if y >= 3: safe(s, y, 4, f"  Average:  {fmt_b(avg)}/s", mx=mx)


def tui(stdscr, cfg, stats):
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.timeout(1000 // cfg["hz"])
    init_colors()

    view = 1
    scroll = 0
    paused = False

    while True:
        k = stdscr.getch()
        if k in (ord("q"), ord("Q")): break
        elif k in (ord("p"), ord("P")): paused = not paused
        elif k == curses.KEY_F1: view = 1; scroll = 0
        elif k == curses.KEY_F2: view = 2; scroll = 0
        elif k == curses.KEY_F3: view = 3; scroll = 0
        elif k == curses.KEY_UP: scroll = max(0, scroll - 1)
        elif k == curses.KEY_DOWN: scroll += 1

        if paused:
            time.sleep(0.1)
            continue

        snap = stats.snapshot()
        stdscr.erase()
        my, mx = stdscr.getmaxyx()

        # Header
        title = " OnDemand DDS Monitor "
        safe(stdscr, 0, max(0, (mx - len(title)) // 2), title, curses.color_pair(C_HDR) | curses.A_BOLD, mx)
        x = 2
        for i, name in enumerate(VIEWS):
            vid = i + 1
            attr = (curses.color_pair(C_HDR) | curses.A_BOLD) if vid == view else curses.color_pair(C_CYA)
            label = f" [F{vid}] {name} "
            safe(stdscr, 1, x, label, attr, mx)
            x += len(label) + 1

        # Footer
        pause_str = " [PAUSED]" if paused else ""
        footer = f" iface={cfg['iface']}  domain={cfg['domain']}{pause_str}  [Q]Quit [P]Pause [↑↓]Scroll"
        safe(stdscr, my - 1, 0, footer[:mx], curses.color_pair(C_DIM), mx)

        if view == 1: draw_overview(stdscr, snap, cfg, scroll, my, mx)
        elif view == 2: draw_writers(stdscr, snap, cfg, scroll, my, mx)
        elif view == 3: draw_protocol(stdscr, snap, cfg, scroll, my, mx)

        stdscr.refresh()


# ═══════════════════════════════════════════════════════════════
#  入口
# ═══════════════════════════════════════════════════════════════
def print_stats(stats, duration=5, interval=1):
    """非交互模式: 终端打印统计"""
    print(f"Monitoring for {duration}s (Ctrl+C to stop)...")
    try:
        for i in range(duration):
            time.sleep(interval)
            snap = stats.snapshot()
            up = snap["uptime"]
            print(f"\n--- t={up:.1f}s ---")
            print(f"  Packets: {snap['pkt_total']:>8,}  RTPS: {snap['pkt_rtps']:>8,}  Bytes: {fmt_b(snap['bytes_total'])}")
            print(f"  Rates:   {fmt_r(snap['pkt_rate'])}/s pkt  {fmt_b(snap['byte_rate'])}/s byte  {fmt_r(snap['data_rate'])}/s data")
            print(f"  Ports:   {snap['port_counts']}")
            names = {0x06:"ACKNACK",0x07:"HEARTBEAT",0x08:"GAP",0x09:"INFO_TS",0x0e:"INFO_DST",0x15:"DATA",0x16:"DATA_FRAG",0x80:"VENDOR"}
            sub = ", ".join(f"{names.get(k,f'0x{k:02x}')}={v}" for k,v in sorted(snap["submsg_counts"].items(), key=lambda x:-x[1]))
            print(f"  Submsgs: {sub}")
            print(f"  Writers: {len(snap['sorted_user'])} user, {len(snap['builtin_writers'])} built-in")
            for wid, info in snap["sorted_user"][:3]:
                print(f"    {wid}: {info['count']} pkts, {fmt_r(info['rate'])}/s")
            if len(snap["sorted_user"]) > 3:
                print(f"    ... and {len(snap['sorted_user'])-3} more")
    except KeyboardInterrupt:
        pass
    snap = stats.snapshot()
    print(f"\n=== Final ({snap['uptime']:.1f}s) ===")
    print(f"  Total: {snap['pkt_total']:,} pkts  {snap['pkt_rtps']:,} RTPS  {fmt_b(snap['bytes_total'])}")
    print(f"  Writers: {len(snap['sorted_user'])} user, {len(snap['builtin_writers'])} built-in")


def main():
    p = argparse.ArgumentParser(description="OnDemand DDS Monitor")
    p.add_argument("-i", "--interface", default="lo", help="网卡名")
    p.add_argument("-d", "--domain", type=int, default=66, help="DDS domain")
    p.add_argument("--hz", type=int, default=2, help="刷新频率")
    p.add_argument("--debug", action="store_true", help="抓所有UDP")
    p.add_argument("--print", action="store_true", help="非交互模式(终端打印)")
    p.add_argument("--duration", type=int, default=10, help="非交互模式持续时间(s)")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    level = logging.DEBUG if args.verbose else logging.WARNING
    logging.basicConfig(level=level, filename="monitor.log", filemode="w",
                        format="[%(asctime)s] %(message)s")

    ports = None if args.debug else {7400, 7401}
    cfg = {"iface": args.interface, "domain": args.domain, "hz": args.hz}

    stats = Stats()
    pcap = PcapWorker(args.interface, stats, port_filter=ports)
    pcap.start()
    time.sleep(0.3)

    logging.info("started iface=%s domain=%d ports=%s", args.interface, args.domain, ports)

    try:
        if args.print:
            print_stats(stats, duration=args.duration)
        else:
            curses.wrapper(lambda s: tui(s, cfg, stats))
    except KeyboardInterrupt:
        pass
    finally:
        pcap.stop()

if __name__ == "__main__":
    main()
