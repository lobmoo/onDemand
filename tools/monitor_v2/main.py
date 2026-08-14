#!/usr/bin/env python3
"""OnDemand DDS Monitor v2.0 - Entry point"""
import argparse
import logging
import os
import sys

from config import MonitorConfig
from core.metrics import MetricsEngine
from core.pcap import PcapWorker
from tui.app import MonitorApp


def parse_args():
    parser = argparse.ArgumentParser(
        description="OnDemand DDS Monitor v2.0",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  sudo python3 main.py -i enx0826ae3a20b7
  sudo python3 main.py -i enx0826ae3a20b7 -d 166
  sudo python3 main.py -i lo --debug
        """,
    )
    parser.add_argument("-i", "--interface", default="lo", help="Network interface")
    parser.add_argument("-d", "--domain", type=int, default=-1, help="DDS domain ID (-1=auto)")
    parser.add_argument("--hz", type=int, default=4, help="TUI refresh rate")
    parser.add_argument("--debug", action="store_true", help="Debug mode")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose logging")
    return parser.parse_args()


def main():
    args = parse_args()

    # Check root
    if os.geteuid() != 0:
        print("Error: raw socket requires root. Use sudo.", file=sys.stderr)
        print(f"  sudo python3 {sys.argv[0]} -i {args.interface}", file=sys.stderr)
        sys.exit(1)

    # Logging
    level = logging.DEBUG if args.verbose else logging.WARNING
    logging.basicConfig(
        level=level, filename="monitor_v2.log", filemode="w",
        format="[%(asctime)s] [%(levelname)s] %(name)s: %(message)s",
    )

    # Config
    config = MonitorConfig(
        interface=args.interface,
        domain_id=args.domain,
        refresh_hz=args.hz,
        debug=args.debug,
        verbose=args.verbose,
    )

    # Metrics engine
    metrics = MetricsEngine(
        window_sec=config.rate_window_sec,
        max_packets=config.max_packets,
    )

    # Packet capture
    ports = None if args.debug else config.ports
    pcap = PcapWorker(
        iface=config.interface,
        metrics=metrics,
        port_filter=ports,
        domain_filter=config.domain_id,
    )

    print(f"[INFO] Starting OnDemand DDS Monitor v2.0", file=sys.stderr)
    print(f"[INFO] Interface: {config.interface}", file=sys.stderr)
    print(f"[INFO] Domain: {'auto-discover' if config.domain_id < 0 else config.domain_id}", file=sys.stderr)
    print(f"[INFO] Ports: {'all' if args.debug else config.ports}", file=sys.stderr)

    pcap.start()

    try:
        app = MonitorApp(config, metrics, pcap)
        app.run()
    except KeyboardInterrupt:
        pass
    finally:
        pcap.stop()
        print("[INFO] Monitor stopped.", file=sys.stderr)


if __name__ == "__main__":
    main()