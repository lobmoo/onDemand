"""OnDemand DDS Monitor v2.0 - Metrics engine"""
import threading
import time
from collections import defaultdict, deque
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


class RateMeter:
    """Sliding window rate meter (thread-safe)"""

    def __init__(self, window: float = 2.0):
        self._window = window
        self._events: deque = deque()
        self._lock = threading.Lock()

    def add(self, ts: Optional[float] = None, val: float = 1.0):
        if ts is None:
            ts = time.time()
        with self._lock:
            self._events.append((ts, val))
            self._evict(ts)

    def _evict(self, now: float):
        cutoff = now - self._window
        while self._events and self._events[0][0] < cutoff:
            self._events.popleft()

    def rate(self) -> float:
        with self._lock:
            if len(self._events) < 2:
                return 0.0
            dt = self._events[-1][0] - self._events[0][0]
            return (len(self._events) - 1) / dt if dt > 0 else 0.0

    def count(self) -> int:
        with self._lock:
            return len(self._events)

    def sum_values(self) -> float:
        with self._lock:
            return sum(v for _, v in self._events)

    def mean(self) -> float:
        with self._lock:
            if not self._events:
                return 0.0
            return sum(v for _, v in self._events) / len(self._events)

    def reset(self):
        with self._lock:
            self._events.clear()


class SNTracker:
    """RTPS sequence number tracker for packet loss detection"""

    def __init__(self):
        self._last_sn: Dict[str, int] = {}
        self._expected: Dict[str, int] = {}
        self._received: Dict[str, int] = defaultdict(int)
        self._lost: Dict[str, int] = defaultdict(int)
        self._gaps: Dict[str, List[Tuple[int, int]]] = defaultdict(list)
        self._lock = threading.Lock()

    def update(self, writer_id: str, sn: int):
        with self._lock:
            self._received[writer_id] += 1
            if writer_id in self._expected:
                if sn > self._expected[writer_id]:
                    lost_count = sn - self._expected[writer_id]
                    self._lost[writer_id] += lost_count
                    self._gaps[writer_id].append((self._expected[writer_id], sn))
                    if len(self._gaps[writer_id]) > 100:
                        self._gaps[writer_id].pop(0)
            self._expected[writer_id] = sn + 1
            self._last_sn[writer_id] = sn

    def report(self) -> dict:
        with self._lock:
            result = {}
            for wid in set(list(self._received.keys()) + list(self._lost.keys())):
                recv = self._received.get(wid, 0)
                lost = self._lost.get(wid, 0)
                total = recv + lost
                result[wid] = {
                    "received": recv, "lost": lost,
                    "loss_rate": lost / total if total > 0 else 0.0,
                    "last_sn": self._last_sn.get(wid, 0),
                    "recent_gaps": list(self._gaps.get(wid, []))[-5:],
                }
            return result

    def reset(self):
        with self._lock:
            self._last_sn.clear()
            self._expected.clear()
            self._received.clear()
            self._lost.clear()
            self._gaps.clear()


@dataclass
class DomainInfo:
    domain_id: int
    participants: set = field(default_factory=set)
    ports: dict = field(default_factory=dict)
    first_seen: float = 0.0
    last_seen: float = 0.0
    pkt_count: int = 0


@dataclass
class TopicInfo:
    name: str
    type_name: str = ""
    writer_id: str = ""
    first_seen: float = 0.0
    last_seen: float = 0.0
    pkt_count: int = 0
    byte_count: int = 0
    gap_count: int = 0
    frag_count: int = 0
    loss_rate: float = 0.0
    lost_count: int = 0
    received_count: int = 0


class MetricsEngine:
    """Central metrics engine aggregating all layers"""

    def __init__(self, window_sec: float = 2.0, max_packets: int = 500):
        self._lock = threading.Lock()
        self._start_time = time.time()
        self._window_sec = window_sec

        # Global packet stats
        self.pkt_total = 0
        self.pkt_rtps = 0
        self.bytes_total = 0
        self.pkt_rate = RateMeter(window_sec)
        self.byte_rate = RateMeter(window_sec)
        self.data_rate = RateMeter(window_sec)

        # Submessage counts
        self.submsg_counts: Dict[int, int] = defaultdict(int)
        self.port_counts: Dict[int, int] = defaultdict(int)

        # Domains
        self.domains: Dict[int, DomainInfo] = {}

        # Writers
        self.data_writers: Dict[str, dict] = defaultdict(
            lambda: {"count": 0, "bytes": 0, "rate": RateMeter(window_sec)}
        )
        self.spdp_count = 0
        self.sedp_pub_count = 0
        self.sedp_sub_count = 0

        # Topics
        self.topics: Dict[str, TopicInfo] = {}
        self.topic_pkt_count: Dict[str, int] = defaultdict(int)
        self.topic_byte_count: Dict[str, int] = defaultdict(int)
        self.topic_rate: Dict[str, RateMeter] = defaultdict(lambda: RateMeter(window_sec))

        # SN loss tracking
        self.sn_tracker = SNTracker()

        # OnDemand layer
        self.known_vars: Dict[int, dict] = {}
        self.known_subs: Dict[str, dict] = {}
        self.pub_table_defines: list = []
        self.sub_registers: list = []
        self.data_transfers: list = []
        self.bucket_var_count: List[int] = [0] * 20

        # Events & raw packets
        self.events: deque = deque(maxlen=100)
        self.rtps_packets: deque = deque(maxlen=max_packets)

    def on_packet(self, pkt_len: int, sport: int = 0, dport: int = 0):
        now = time.time()
        with self._lock:
            self.pkt_total += 1
            self.bytes_total += pkt_len
            self.pkt_rate.add(now)
            self.byte_rate.add(now, pkt_len)
            for port in (sport, dport):
                if port in (7400, 7401):
                    self.port_counts[port] += 1

    def on_rtps(self):
        with self._lock:
            self.pkt_rtps += 1

    def on_submsg(self, sid: int, wid_hex: str = "", frag_len: int = 0):
        now = time.time()
        with self._lock:
            self.submsg_counts[sid] += 1
            if sid in (0x15, 0x16):
                self.data_rate.add(now)
                if wid_hex:
                    w = self.data_writers[wid_hex]
                    w["count"] += 1
                    w["bytes"] += frag_len
                    w["rate"].add(now)
                    if wid_hex == "000100c2":
                        self.spdp_count += 1
                    elif wid_hex == "000003c2":
                        self.sedp_pub_count += 1
                    elif wid_hex == "000004c2":
                        self.sedp_sub_count += 1

    def on_domain_discovered(self, domain_id: int, guid_prefix: bytes,
                              sport: int, dport: int, pkt_len: int):
        now = time.time()
        with self._lock:
            if domain_id not in self.domains:
                self.domains[domain_id] = DomainInfo(
                    domain_id=domain_id, first_seen=now, last_seen=now
                )
            d = self.domains[domain_id]
            d.participants.add(guid_prefix[:6])
            d.last_seen = now
            d.pkt_count += 1
            for port in (sport, dport):
                if port not in d.ports:
                    d.ports[port] = {"pkts": 0, "bytes": 0}
                d.ports[port]["pkts"] += 1
                d.ports[port]["bytes"] += pkt_len

    def on_topic_discover(self, topic_name: str, type_name: str, wid_hex: str):
        now = time.time()
        with self._lock:
            if topic_name not in self.topics:
                self.topics[topic_name] = TopicInfo(
                    name=topic_name, type_name=type_name or "",
                    writer_id=wid_hex, first_seen=now, last_seen=now
                )
                self.events.append(f"[SEDP] {topic_name} -> {wid_hex}")
            else:
                self.topics[topic_name].last_seen = now
                self.topics[topic_name].writer_id = wid_hex

    def on_topic_packet(self, topic_name: str, pkt_len: int):
        now = time.time()
        with self._lock:
            self.topic_pkt_count[topic_name] += 1
            self.topic_byte_count[topic_name] += pkt_len
            self.topic_rate[topic_name].add(now)
            if topic_name in self.topics:
                t = self.topics[topic_name]
                t.pkt_count += 1
                t.byte_count += pkt_len
                t.last_seen = now

    def on_gap_event(self, writer_id: str):
        with self._lock:
            for tinfo in self.topics.values():
                if tinfo.writer_id == writer_id:
                    tinfo.gap_count += 1
                    break

    def on_frag_event(self, writer_id: str):
        with self._lock:
            for tinfo in self.topics.values():
                if tinfo.writer_id == writer_id:
                    tinfo.frag_count += 1
                    break

    def on_sn_update(self, writer_id: str, sn: int):
        self.sn_tracker.update(writer_id, sn)
        report = self.sn_tracker.report()
        with self._lock:
            for tinfo in self.topics.values():
                if tinfo.writer_id in report:
                    r = report[tinfo.writer_id]
                    tinfo.loss_rate = r["loss_rate"]
                    tinfo.lost_count = r["lost"]
                    tinfo.received_count = r["received"]

    def on_pub_table_define(self, decoded: dict):
        now = time.time()
        with self._lock:
            self.pub_table_defines.append({"time": now, "data": decoded})
            if len(self.pub_table_defines) > 50:
                self.pub_table_defines.pop(0)
            node = decoded.get("nodeName", "")
            for v in decoded.get("vars", []):
                vid = v.get("id", 0)
                if v.get("removed"):
                    self.known_vars.pop(vid, None)
                else:
                    self.known_vars[vid] = {
                        "name": v.get("name", ""), "nodeName": node,
                        "size": v.get("size", 0), "bucket": vid % 20,
                        "table": decoded.get("name", ""),
                    }
            self.bucket_var_count = [0] * 20
            for v in self.known_vars.values():
                b = v.get("bucket", 0)
                if 0 <= b < 20:
                    self.bucket_var_count[b] += 1
            self.events.append(f"[DEFINE] {node}/{decoded.get('name','')}: {decoded.get('var_count',0)} vars")

    def on_sub_register(self, decoded: dict):
        now = time.time()
        with self._lock:
            self.sub_registers.append({"time": now, "data": decoded})
            if len(self.sub_registers) > 50:
                self.sub_registers.pop(0)
            node = decoded.get("nodeName", "")
            self.known_subs[node] = {
                "vars": decoded.get("vars", []),
                "tableName": decoded.get("tableName", ""),
                "last_seen": now, "msgType": decoded.get("msgType", 0),
            }
            self.events.append(f"[SUB] {node} -> {decoded.get('tableName','')}: {decoded.get('var_count',0)} vars")

    def snapshot(self) -> dict:
        now = time.time()
        with self._lock:
            uptime = now - self._start_time
            builtin_writers = {}
            user_writers = {}
            for wid, info in self.data_writers.items():
                entry = {"count": info["count"], "bytes": info["bytes"],
                         "rate": info["rate"].rate()}
                kind = int(wid[-2:], 16)
                if kind == 0xc2:
                    builtin_writers[wid] = entry
                elif kind == 0x03:
                    ekey = int(wid[:6], 16)
                    entry["bucket"] = ekey - 3
                    user_writers[wid] = entry
            sorted_user = sorted(user_writers.items(), key=lambda x: int(x[0][:6], 16))
            topic_list = []
            for tname, tinfo in sorted(self.topics.items()):
                topic_list.append({
                    "name": tname, "type_name": tinfo.type_name,
                    "writer_id": tinfo.writer_id, "pkt_count": tinfo.pkt_count,
                    "byte_count": tinfo.byte_count,
                    "rate": self.topic_rate[tname].rate() if tname in self.topic_rate else 0.0,
                    "avg_size": tinfo.byte_count / max(1, tinfo.pkt_count),
                    "gap_count": tinfo.gap_count, "frag_count": tinfo.frag_count,
                    "loss_rate": tinfo.loss_rate,
                    "lost_count": tinfo.lost_count, "received_count": tinfo.received_count,
                })
            domain_list = []
            for did, dinfo in sorted(self.domains.items()):
                domain_list.append({
                    "domain_id": did, "participants": len(dinfo.participants),
                    "ports": dict(dinfo.ports), "first_seen": dinfo.first_seen,
                    "last_seen": dinfo.last_seen, "pkt_count": dinfo.pkt_count,
                })
            return {
                "uptime": uptime, "pkt_total": self.pkt_total,
                "pkt_rtps": self.pkt_rtps, "bytes_total": self.bytes_total,
                "pkt_rate": self.pkt_rate.rate(), "byte_rate": self.byte_rate.rate(),
                "data_rate": self.data_rate.rate(),
                "submsg_counts": dict(self.submsg_counts),
                "port_counts": dict(self.port_counts),
                "sorted_user": sorted_user, "builtin_writers": builtin_writers,
                "spdp_count": self.spdp_count, "sedp_pub_count": self.sedp_pub_count,
                "sedp_sub_count": self.sedp_sub_count,
                "domain_list": domain_list, "topic_list": topic_list,
                "events": list(self.events),
                "pub_table_defines": list(self.pub_table_defines),
                "sub_registers": list(self.sub_registers),
                "known_vars": dict(self.known_vars),
                "known_subs": dict(self.known_subs),
                "total_vars": len(self.known_vars),
                "bucket_var_count": list(self.bucket_var_count),
                "rtps_packets": list(self.rtps_packets),
                "sn_report": self.sn_tracker.report(),
            }

    def reset(self):
        with self._lock:
            self._start_time = time.time()
            self.pkt_total = self.pkt_rtps = self.bytes_total = 0
            self.pkt_rate = RateMeter(self._window_sec)
            self.byte_rate = RateMeter(self._window_sec)
            self.data_rate = RateMeter(self._window_sec)
            self.submsg_counts.clear()
            self.port_counts.clear()
            self.domains.clear()
            self.data_writers.clear()
            self.spdp_count = self.sedp_pub_count = self.sedp_sub_count = 0
            self.topics.clear()
            self.topic_pkt_count.clear()
            self.topic_byte_count.clear()
            self.topic_rate.clear()
            self.sn_tracker.reset()
            self.events.clear()
            self.rtps_packets.clear()
            self.known_vars.clear()
            self.known_subs.clear()
            self.pub_table_defines.clear()
            self.sub_registers.clear()
            self.data_transfers.clear()
            self.bucket_var_count = [0] * 20