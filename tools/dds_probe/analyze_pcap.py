import struct
data = open('/home/workspaces/onDemand/capture2.pcap', 'rb').read()
# pcap global header: 24 bytes
# Each packet: 16 byte header + data
offset = 24
pkt_count = 0
rtps_count = 0
submsg_ids = {}
while offset < len(data):
    if offset + 16 > len(data): break
    ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', data[offset:offset+16])
    pkt_data = data[offset+16:offset+16+incl_len]
    offset += 16 + incl_len
    pkt_count += 1
    # Find RTPS magic in packet
    idx = pkt_data.find(b'RTPS')
    if idx >= 0 and idx < 100:
        rtps_count += 1
        ptr = idx + 20
        end = len(pkt_data)
        while ptr + 4 <= end:
            sm_id = pkt_data[ptr]
            flags = pkt_data[ptr+1]
            otn = struct.unpack('<H', pkt_data[ptr+2:ptr+4])[0] if (flags & 1) else struct.unpack('>H', pkt_data[ptr+2:ptr+4])[0]
            submsg_ids[sm_id] = submsg_ids.get(sm_id, 0) + 1
            if otn == 0: break
            ptr += 4 + otn
print(f'Total packets: {pkt_count}')
print(f'RTPS packets: {rtps_count}')
print('Submsg IDs:')
names = {0x15:'DATA',0x16:'DATA_FRAG',0x07:'HEARTBEAT',0x06:'ACKNACK',0x08:'GAP',0x09:'INFO_TS',0x0e:'INFO_DST',0x0c:'INFO_SRC'}
for k, v in sorted(submsg_ids.items()):
    name = names.get(k, '???')
    print(f'  0x{k:02x} {name:15s} {v}')
