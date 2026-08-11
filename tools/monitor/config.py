"""
OnDemand DDS Monitor - 配置模块
"""

from dataclasses import dataclass, field


@dataclass
class MonitorConfig:
    """监控工具全局配置"""

    # ── 网络抓包 ──
    interface: str = "lo"  # 抓包网卡 (默认 lo 用于本地测试)
    bpf_filter: str = "udp and (port 7400 or port 7401)"

    # ── DDS ──
    domain_id: int = 66  # DDS domain (与 onDemand 一致)

    # ── TUI 刷新 ──
    refresh_hz: int = 2  # TUI 刷新频率 (Hz)

    # ── 告警阈值 ──
    hb_loss_warn: float = 0.01  # 1%
    hb_loss_crit: float = 0.05  # 5%
    resend_warn: float = 0.005  # 0.5%
    resend_crit: float = 0.02  # 2%
    jitter_warn_ms: float = 1.0  # 1ms
    jitter_crit_ms: float = 5.0  # 5ms

    # ── 滑动窗口 ──
    rate_window_sec: float = 1.0  # 速率计算窗口
    jitter_window_size: int = 100  # jitter 采样窗口

    # ── 调试 ──
    debug: bool = False  # 调试模式: 宽松过滤 + 详细日志

    # ── RTPS 端口 ──
    rtps_discovery_port: int = 7400  # RTPS discovery multicast port
    rtps_data_port: int = 7401  # RTPS user data multicast port

    # ── Topic 名称 (与 onDemand 一致) ──
    topic_table_define: str = "dsf/sys/var/tableDefine"
    topic_sub_register: str = "dsf/message/commandRequest/subTableRegister"
    topic_data_prefix: str = "dsf/var/data/transfer/bucket_"

    # ── Bucket 配置 ──
    bucket_count: int = 20  # ONDEMAND_BUCKET_SIZE
