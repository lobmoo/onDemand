"""OnDemand DDS Monitor v2.0 - 配置管理"""
from dataclasses import dataclass, field
from typing import Set


@dataclass
class MonitorConfig:
    """监控工具全局配置"""
    interface: str = "lo"
    ports: Set[int] = field(default_factory=lambda: {7400, 7401})
    domain_id: int = -1  # -1 = auto discover
    refresh_hz: int = 4
    max_packets: int = 500
    rate_window_sec: float = 2.0
    hb_loss_warn: float = 0.01
    hb_loss_crit: float = 0.05
    resend_warn: float = 0.005
    resend_crit: float = 0.02
    loss_warn: float = 0.001
    loss_crit: float = 0.01
    debug: bool = False
    verbose: bool = False
