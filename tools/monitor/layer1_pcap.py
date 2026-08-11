"""
OnDemand DDS Monitor - Layer 1: 网络抓包层

用 scapy 抓 UDP 包，解析 RTPS 子消息，统计网络层指标。
"""

import logging
import sys
import threading
import time

# 确保 /usr/local/lib 路径可用 (Docker 环境)
if "/usr/local/lib/python3.8/dist-packages" not in sys.path:
    sys.path.insert(0, "/usr/local/lib/python3.8/dist-packages")

from scapy.all import IP, UDP, sniff

try:
    from .config import MonitorConfig
    from .metrics import MetricsEngine
    from .rtps_parser import (
        RTPSParser,
        SUBMSG_ID_ACKNACK,
        SUBMSG_ID_DATA,
        SUBMSG_ID_GAP,
        SUBMSG_ID_HEARTBEAT,
    )
except ImportError:
    from config import MonitorConfig
    from metrics import MetricsEngine
    from rtps_parser import (
        RTPSParser,
        SUBMSG_ID_ACKNACK,
        SUBMSG_ID_DATA,
        SUBMSG_ID_GAP,
        SUBMSG_ID_HEARTBEAT,
    )

logger = logging.getLogger(__name__)


class PcapWorker:
    """抓包工作线程"""

    def __init__(self, config: MonitorConfig, metrics: MetricsEngine):
        self.config = config
        self.metrics = metrics
        self._running = threading.Event()
        self._thread: threading.Thread | None = None
        self._pkt_count = 0
        self._rtps_count = 0
        self._non_rtps_count = 0

    def start(self):
        """启动抓包线程"""
        if self._running.is_set():
            return
        self._running.set()
        self._thread = threading.Thread(target=self._run, daemon=True, name="pcap-worker")
        self._thread.start()
        logger.info(
            "PcapWorker started: interface=%s filter=%s",
            self.config.interface,
            self.config.bpf_filter,
        )

    def stop(self):
        """停止抓包"""
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=3)
            self._thread = None
        logger.info(
            "PcapWorker stopped: total=%d rtps=%d non_rtps=%d",
            self._pkt_count,
            self._rtps_count,
            self._non_rtps_count,
        )

    def _run(self):
        """抓包主循环"""
        try:
            # 先用宽松过滤只看 UDP，调试用
            bpf = self.config.bpf_filter
            if self.config.debug:
                bpf = "udp"  # 调试模式: 抓所有 UDP
                logger.info("Debug mode: capturing ALL UDP packets")
            logger.info("Starting sniff: iface=%s filter=%s", self.config.interface, bpf)
            sniff(
                iface=self.config.interface,
                filter=bpf,
                prn=self._process_packet,
                store=False,
                stop_filter=lambda _: not self._running.is_set(),
            )
        except PermissionError:
            logger.error(
                "Permission denied: 需要 root 权限抓包. 请用 sudo 运行, 或设置 CAP_NET_RAW."
            )
        except Exception as e:
            logger.error("PcapWorker error: %s", e)

    def _process_packet(self, pkt):
        """处理单个抓到的包"""
        if not self._running.is_set():
            return

        self._pkt_count += 1

        # 提取 UDP payload
        if not pkt.haslayer(UDP) or not pkt.haslayer(IP):
            return

        payload = bytes(pkt[UDP].payload)
        if len(payload) < 4:
            return

        # 调试: 每 100 个包打印一次摘要
        if self.config.debug and self._pkt_count % 100 == 1:
            src = pkt[IP].src
            dst = pkt[IP].dst
            sport = pkt[UDP].sport
            dport = pkt[UDP].dport
            logger.debug(
                "Pkt #%d: %s:%d -> %s:%d len=%d first4=%s",
                self._pkt_count, src, sport, dst, dport, len(payload),
                payload[:4].hex(),
            )

        # 搜索 RTPS magic (可能不在 payload 开头)
        rtps_offset = payload.find(b"RTPS")
        if rtps_offset == -1:
            self._non_rtps_count += 1
            return

        # RTPS magic 找到了，从该偏移开始解析
        self._rtps_count += 1
        pkt_len = len(payload)
        rtps_payload = payload[rtps_offset:]

        # 解析 RTPS 包
        result = RTPSParser.parse_packet(rtps_payload)

        # 提取 topic (从 DATA 子消息)
        topics = result.get("topics", [])
        topic = topics[0] if topics else None

        # 更新指标
        self.metrics.on_rtps_packet(pkt_len, topic)

        # 统计子消息类型
        for submsg_id in result.get("submsg_types", []):
            if submsg_id == SUBMSG_ID_HEARTBEAT:
                self.metrics.on_heartbeat()
            elif submsg_id == SUBMSG_ID_ACKNACK:
                self.metrics.on_acknack()
            elif submsg_id == SUBMSG_ID_GAP:
                self.metrics.on_gap()
            elif submsg_id == SUBMSG_ID_DATA:
                self.metrics.on_data_msg()

    def is_alive(self) -> bool:
        return self._thread is not None and self._thread.is_alive()
