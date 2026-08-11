#!/usr/bin/env python3
"""
OnDemand DDS Monitor - 主入口

分层监控工具:
  Layer 1: scapy 抓包 + RTPS 解析
  Layer 2: fastdds Python 绑定 + DDS topic 订阅
  Layer 3: rich + curses 交互终端

用法:
  sudo python -m tools.monitor.main --interface lo --domain 66
  sudo python tools/monitor/main.py --interface eth0
"""

import argparse
import logging
import signal
import sys
import time

try:
    from .config import MonitorConfig
    from .layer1_pcap import PcapWorker
    from .layer2_dds import DdsWorker
    from .layer3_tui import MonitorTUI
    from .metrics import MetricsEngine
except ImportError:
    # 直接运行 (非 -m 模式)
    from config import MonitorConfig
    from layer1_pcap import PcapWorker
    from layer2_dds import DdsWorker
    from layer3_tui import MonitorTUI
    from metrics import MetricsEngine


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="OnDemand DDS Monitor - 分层监控工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  sudo python -m tools.monitor.main --interface lo --domain 66
  sudo python tools/monitor/main.py --interface eth0 --refresh 5
        """,
    )
    parser.add_argument(
        "-i", "--interface",
        default="lo",
        help="抓包网卡 (默认: lo)",
    )
    parser.add_argument(
        "-d", "--domain",
        type=int,
        default=66,
        help="DDS domain ID (默认: 66)",
    )
    parser.add_argument(
        "-r", "--refresh",
        type=int,
        default=2,
        help="TUI 刷新频率 Hz (默认: 2)",
    )
    parser.add_argument(
        "--no-dds",
        action="store_true",
        help="禁用 DDS 层 (只用抓包层)",
    )
    parser.add_argument(
        "--no-pcap",
        action="store_true",
        help="禁用抓包层 (只用 DDS 层)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="详细日志",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="调试模式: 宽松 BPF 过滤 + 详细抓包日志",
    )
    return parser.parse_args()


def setup_logging(verbose: bool, debug: bool):
    """配置日志 - 输出到文件, 不干扰 curses TUI"""
    level = logging.DEBUG if (verbose or debug) else logging.WARNING
    # 日志写到文件, 终端留给 curses
    logging.basicConfig(
        level=level,
        format="[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s",
        datefmt="%H:%M:%S",
        filename="monitor.log",
        filemode="w",
    )


def main():
    """主函数"""
    args = parse_args()
    setup_logging(args.verbose, args.debug)

    logger = logging.getLogger(__name__)
    logger.info("Starting OnDemand DDS Monitor")

    # 构建配置
    config = MonitorConfig(
        interface=args.interface,
        domain_id=args.domain,
        refresh_hz=args.refresh,
        debug=args.debug,
    )

    # 创建指标引擎
    metrics = MetricsEngine(
        window_sec=config.rate_window_sec,
        jitter_window=config.jitter_window_size,
    )

    # 启动各层
    pcap_worker = None
    dds_worker = None
    tui = None

    try:
        # Layer 1: 抓包
        if not args.no_pcap:
            pcap_worker = PcapWorker(config, metrics)
            pcap_worker.start()

        # Layer 2: DDS
        if not args.no_dds:
            dds_worker = DdsWorker(config, metrics)
            dds_worker.start()

        # Layer 3: TUI (阻塞主线程)
        tui = MonitorTUI(config, metrics)
        tui.start()

        # 等待 TUI 退出
        while tui.state.running:
            time.sleep(0.1)

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        # 清理
        logger.info("Shutting down...")
        if tui:
            tui.stop()
        if pcap_worker:
            pcap_worker.stop()
        if dds_worker:
            dds_worker.stop()
        logger.info("Monitor stopped.")


if __name__ == "__main__":
    main()
