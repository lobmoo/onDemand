"""
OnDemand DDS Monitor - 指标计算引擎

滑动窗口统计、速率计算、jitter 计算、统计快照。
"""

import threading
import time
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Tuple


class SlidingWindow:
    """时间窗口滑动统计"""

    def __init__(self, window_sec: float = 1.0):
        self.window_sec = window_sec
        self._events: List[Tuple[float, float]] = []  # [(timestamp, value), ...]
        self._lock = threading.Lock()

    def add(self, ts: float, value: float = 1.0):
        with self._lock:
            self._events.append((ts, value))
            self._evict(ts)

    def _evict(self, now: float):
        cutoff = now - self.window_sec
        while self._events and self._events[0][0] < cutoff:
            self._events.pop(0)

    def count(self) -> int:
        with self._lock:
            return len(self._events)

    def rate(self) -> float:
        """返回每秒速率"""
        with self._lock:
            if len(self._events) < 2:
                return 0.0
            duration = self._events[-1][0] - self._events[0][0]
            if duration <= 0:
                return 0.0
            return (len(self._events) - 1) / duration

    def sum_values(self) -> float:
        with self._lock:
            return sum(v for _, v in self._events)

    def mean(self) -> float:
        with self._lock:
            if not self._events:
                return 0.0
            return sum(v for _, v in self._events) / len(self._events)


class JitterCalculator:
    """滑动窗口 jitter 计算器 (标准差)"""

    def __init__(self, window_size: int = 100):
        self.window_size = window_size
        self._timestamps: List[float] = []
        self._lock = threading.Lock()

    def add(self, ts_sec: float, ts_nsec: float):
        ts = ts_sec + ts_nsec * 1e-9
        with self._lock:
            self._timestamps.append(ts)
            if len(self._timestamps) > self.window_size:
                self._timestamps.pop(0)

    def jitter_ms(self) -> float:
        """返回标准差 jitter (ms)"""
        with self._lock:
            if len(self._timestamps) < 2:
                return 0.0
            deltas = [
                self._timestamps[i] - self._timestamps[i - 1]
                for i in range(1, len(self._timestamps))
            ]
            mean = sum(deltas) / len(deltas)
            variance = sum((d - mean) ** 2 for d in deltas) / len(deltas)
            return (variance**0.5) * 1000  # 转换为 ms

    def last_ts(self) -> float:
        with self._lock:
            return self._timestamps[-1] if self._timestamps else 0.0


class Counter:
    """线程安全计数器"""

    def __init__(self):
        self._value = 0
        self._lock = threading.Lock()

    def inc(self, n: int = 1):
        with self._lock:
            self._value += n

    def value(self) -> int:
        with self._lock:
            return self._value

    def reset(self):
        with self._lock:
            self._value = 0


@dataclass
class StatsSnapshot:
    """某一时刻的全部统计快照，供 TUI 渲染"""

    timestamp: float = 0.0
    uptime: float = 0.0

    # ── Layer 1: 网络 ──
    pkt_count: int = 0
    pkt_rate: float = 0.0
    byte_count: int = 0
    throughput: float = 0.0  # bytes/s
    hb_count: int = 0
    ack_count: int = 0
    gap_count: int = 0
    data_count: int = 0
    hb_loss_rate: float = 0.0
    resend_rate: float = 0.0
    topic_stats: dict = field(default_factory=dict)  # topic -> {pkts, bytes, rate}

    # ── Layer 2: DDS 应用 ──
    pub_nodes: list = field(default_factory=list)
    sub_nodes: list = field(default_factory=list)
    var_count: int = 0
    bucket_var_count: list = field(default_factory=lambda: [0] * 20)
    sub_freq_dist: dict = field(default_factory=dict)  # freq_ms -> count
    data_msg_rate: list = field(default_factory=lambda: [0.0] * 20)
    jitter_ms: list = field(default_factory=lambda: [0.0] * 20)
    throughput_per_bucket: list = field(default_factory=lambda: [0.0] * 20)
    mask_var_count_avg: list = field(default_factory=lambda: [0.0] * 20)


class MetricsEngine:
    """指标计算引擎：聚合 Layer1 + Layer2 的所有统计"""

    def __init__(self, window_sec: float = 1.0, jitter_window: int = 100):
        self._start_time = time.time()
        self._window_sec = window_sec
        self._lock = threading.Lock()

        # ── Layer 1 计数器 ──
        self._pkt_count = Counter()
        self._byte_count = Counter()
        self._hb_count = Counter()
        self._ack_count = Counter()
        self._gap_count = Counter()
        self._data_count = Counter()

        # ── Layer 1 滑动窗口 ──
        self._pkt_rate = SlidingWindow(window_sec)
        self._throughput = SlidingWindow(window_sec)
        self._topic_pkts: Dict[str, Counter] = defaultdict(Counter)
        self._topic_bytes: Dict[str, Counter] = defaultdict(Counter)
        self._topic_rate: Dict[str, SlidingWindow] = defaultdict(
            lambda: SlidingWindow(window_sec)
        )

        # ── Layer 2 DDS 状态 ──
        self._pub_nodes: Set[str] = set()
        self._sub_nodes: Set[str] = set()
        self._var_count = 0
        self._bucket_var_count = [0] * 20
        self._sub_freq_dist: Dict[int, int] = defaultdict(int)

        # ── Layer 2 数据传输 ──
        self._data_msg_rate = [SlidingWindow(window_sec) for _ in range(20)]
        self._jitter = [JitterCalculator(jitter_window) for _ in range(20)]
        self._bucket_throughput = [SlidingWindow(window_sec) for _ in range(20)]
        self._mask_var_count = [SlidingWindow(window_sec) for _ in range(20)]

        # ── 订阅详情 ──
        self._var_table: Dict[int, dict] = {}  # varHash -> {name, nodeName, bucket, ...}
        self._sub_details: List[dict] = []  # [{node, table, vars, freqs}, ...]
        self._table_define_history: List[dict] = []  # 最近 N 条 tableDefine

    # ── Layer 1 写入接口 ──

    def on_rtps_packet(self, pkt_len: int, topic: Optional[str] = None):
        """收到一个 RTPS 包"""
        now = time.time()
        self._pkt_count.inc()
        self._byte_count.inc(pkt_len)
        self._pkt_rate.add(now)
        self._throughput.add(now, pkt_len)
        if topic:
            self._topic_pkts[topic].inc()
            self._topic_bytes[topic].inc(pkt_len)
            self._topic_rate[topic].add(now)

    def on_heartbeat(self):
        self._hb_count.inc()

    def on_acknack(self):
        self._ack_count.inc()

    def on_gap(self):
        self._gap_count.inc()

    def on_data_msg(self):
        self._data_count.inc()

    # ── Layer 2 写入接口 ──

    def on_table_define(self, node_name: str, table_name: str, var_count: int):
        """收到 PubTableDefine"""
        self._pub_nodes.add(node_name)
        self._table_define_history.append(
            {
                "time": time.time(),
                "node": node_name,
                "table": table_name,
                "var_count": var_count,
            }
        )
        # 只保留最近 100 条
        if len(self._table_define_history) > 100:
            self._table_define_history.pop(0)

    def on_sub_register(self, node_name: str, table_name: str, freqs: dict):
        """收到 SubTableRegister, freqs: {varName: freqMs}"""
        self._sub_nodes.add(node_name)
        for freq_ms in freqs.values():
            self._sub_freq_dist[freq_ms] += 1
        self._sub_details.append(
            {
                "time": time.time(),
                "node": node_name,
                "table": table_name,
                "var_count": len(freqs),
                "freqs": freqs,
            }
        )
        if len(self._sub_details) > 100:
            self._sub_details.pop(0)

    def on_data_transfer(
        self,
        bucket_idx: int,
        var_count: int,
        data_size: int,
        ts_sec: int,
        ts_nsec: int,
    ):
        """收到 TableDataTransfer"""
        if 0 <= bucket_idx < 20:
            now = time.time()
            self._data_msg_rate[bucket_idx].add(now)
            self._jitter[bucket_idx].add(ts_sec, ts_nsec)
            self._bucket_throughput[bucket_idx].add(now, data_size)
            self._mask_var_count[bucket_idx].add(now, var_count)

    def set_var_count(self, count: int):
        self._var_count = count

    def set_bucket_var_count(self, bucket: int, count: int):
        if 0 <= bucket < 20:
            self._bucket_var_count[bucket] = count

    # ── 快照读取 ──

    def snapshot(self) -> StatsSnapshot:
        """生成当前时刻的统计快照 (线程安全)"""
        now = time.time()

        # Layer 1
        pkt_count = self._pkt_count.value()
        byte_count = self._byte_count.value()
        hb = self._hb_count.value()
        ack = self._ack_count.value()
        gap = self._gap_count.value()
        data = self._data_count.value()

        hb_loss = (1.0 - ack / hb) if hb > 0 else 0.0
        resend = (gap / data) if data > 0 else 0.0

        topic_stats = {}
        for topic in set(
            list(self._topic_pkts.keys()) + list(self._topic_bytes.keys())
        ):
            topic_stats[topic] = {
                "pkts": self._topic_pkts[topic].value(),
                "bytes": self._topic_bytes[topic].value(),
                "rate": self._topic_rate[topic].rate(),
            }

        # Layer 2
        return StatsSnapshot(
            timestamp=now,
            uptime=now - self._start_time,
            # Layer 1
            pkt_count=pkt_count,
            pkt_rate=self._pkt_rate.rate(),
            byte_count=byte_count,
            throughput=self._throughput.sum_values(),
            hb_count=hb,
            ack_count=ack,
            gap_count=gap,
            data_count=data,
            hb_loss_rate=hb_loss,
            resend_rate=resend,
            topic_stats=topic_stats,
            # Layer 2
            pub_nodes=list(self._pub_nodes),
            sub_nodes=list(self._sub_nodes),
            var_count=self._var_count,
            bucket_var_count=list(self._bucket_var_count),
            sub_freq_dist=dict(self._sub_freq_dist),
            data_msg_rate=[w.rate() for w in self._data_msg_rate],
            jitter_ms=[j.jitter_ms() for j in self._jitter],
            throughput_per_bucket=[w.sum_values() for w in self._bucket_throughput],
            mask_var_count_avg=[w.mean() for w in self._mask_var_count],
        )

    def reset(self):
        """重置所有统计"""
        self._start_time = time.time()
        self._pkt_count.reset()
        self._byte_count.reset()
        self._hb_count.reset()
        self._ack_count.reset()
        self._gap_count.reset()
        self._data_count.reset()
