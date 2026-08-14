#!/usr/bin/env python3
"""Quick diagnostic: capture SPDP packets and test domain ID parsing"""
import sys, os, socket, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core.rtps import parse_rtps_packet, parse_spdp_domain_id, ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER, RTPS_MAGIC

iface = sys.argv[1] if len(sys.argv) > 1 else "lo"

if os.geteuid() != 0:
    print("Need root: sudo python3 debug_spdp.py <iface>")
    sys.exit(1)

sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(0x0003))
sock.bind((iface, 0))
sock.settimeout(5.0)

print(f"Listening on {iface} for SPDP packets (5s timeout)...")
found = 0

while found < 5:
    try:
        data, _ = sock.recvfrom(65535)
    except socket.timeout:
        print("Timeout - no more packets")
        break

    if len(data) < 42:
        continue
    etype = struct.unpack("!H", data[12:14])[0]
    if etype != 0x0800:
        continue
    ihl = (data[14] & 0x0F) * 4
    if data[23] != 17:
        continue
    udp_start = 14 + ihl
    sport, dport = struct.unpack("!HH", data[udp_start:udp_start+4])
    udp_len = struct.unpack("!H", data[udp_start+4:udp_start+6])[0]
    payload = data[udp_start+8:udp_start+udp_len]

    if len(payload) < 20 or payload[:4] != RTPS_MAGIC:
        continue

    result = parse_rtps_packet(payload)
    if not result.get("valid"):
        continue

    for submsg in result["submsgs"]:
        if submsg["id"] not in (0x15, 0x16):
            continue
        wid = submsg["writer_id"]
        if wid != ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
            continue

        found += 1
        is_be = submsg["is_be"]
        offset = submsg["offset"]
        length = submsg["length"]
        body = offset + 4

        print(f"\n--- SPDP Packet #{found} ---")
        print(f"  {sport} -> {dport}, len={length}, BE={is_be}")
        print(f"  WriterId: {wid}")
        print(f"  Submsg hex (first 80): {payload[offset:offset+80].hex()}")

        # octets_to_inline
        octets = struct.unpack_from(">H" if is_be else "<H", payload, body+2)[0]
        print(f"  octetsToInlineQoS = {octets}")
        print(f"  inline QoS start = body+4+octets = {body+4+octets}")

        did = parse_spdp_domain_id(payload, offset, length, is_be)
        print(f"  Parsed domain_id = {did}")

        if did < 0:
            # Dump more hex for debugging
            inline_start = body + 4 + octets
            print(f"  Inline QoS hex (32 bytes): {payload[inline_start:inline_start+32].hex()}")
            # Try payload after inline QoS
            print(f"  Body hex (first 60): {payload[body:body+60].hex()}")

sock.close()
print(f"\nTotal SPDP packets found: {found}")