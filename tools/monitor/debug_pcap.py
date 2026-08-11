#!/usr/bin/env python3
"""
快速诊断: 测试 scapy 抓包是否正常工作
用法: sudo python3 tools/monitor/debug_pcap.py --interface lo
"""
import sys
if "/usr/local/lib/python3.8/dist-packages" not in sys.path:
    sys.path.insert(0, "/usr/local/lib/python3.8/dist-packages")

import argparse
import time
from scapy.all import sniff, UDP, IP, conf

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--interface", default="lo")
    parser.add_argument("-c", "--count", type=int, default=20, help="抓几个包就停")
    parser.add_argument("-t", "--timeout", type=int, default=10, help="超时秒数")
    parser.add_argument("-f", "--filter", default="udp", help="BPF filter")
    args = parser.parse_args()

    print(f"=== Scapy 抓包诊断 ===")
    print(f"Interface: {args.interface}")
    print(f"Filter:    {args.filter}")
    print(f"Count:     {args.count}")
    print(f"Timeout:   {args.timeout}s")
    print()

    # 列出可用接口
    print(f"可用接口: {conf.ifaces}")
    print()

    pkt_count = [0]
    rtps_count = [0]

    def on_pkt(pkt):
        pkt_count[0] += 1
        has_udp = pkt.haslayer(UDP)
        has_ip = pkt.haslayer(IP)

        if has_udp and has_ip:
            payload = bytes(pkt[UDP].payload)
            src = pkt[IP].src
            dst = pkt[IP].dst
            sport = pkt[UDP].sport
            dport = pkt[UDP].dport
            rtps = b"R.T.P.S." in payload
            if rtps:
                rtps_count[0] += 1
            tag = " [RTPS!]" if rtps else ""
            print(f"  #{pkt_count[0]:3d} {src}:{sport} -> {dst}:{dport} len={len(payload)}{tag}")
        else:
            print(f"  #{pkt_count[0]:3d} non-UDP: {pkt.summary()}")

    try:
        print(f"开始抓包...")
        sniff(
            iface=args.interface,
            filter=args.filter,
            prn=on_pkt,
            store=False,
            count=args.count,
            timeout=args.timeout,
        )
    except PermissionError:
        print("ERROR: 需要 root 权限! 用 sudo 运行")
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    print()
    print(f"=== 结果 ===")
    print(f"总包数:   {pkt_count[0]}")
    print(f"RTPS 包:  {rtps_count[0]}")
    print(f"非 RTPS:  {pkt_count[0] - rtps_count[0]}")

    if pkt_count[0] == 0:
        print()
        print("一个包都没抓到! 可能原因:")
        print(f"  1. 接口 '{args.interface}' 不对, 试试: ip addr show")
        print(f"  2. 没有 UDP 流量, 先启动 demo_exec")
        print(f"  3. Docker 网络隔离, 试试 --interface eth0")
    elif rtps_count[0] == 0:
        print()
        print("抓到 UDP 包但没有 RTPS 流量!")
        print("DDS 可能还没启动, 或者端口不对")

if __name__ == "__main__":
    main()
