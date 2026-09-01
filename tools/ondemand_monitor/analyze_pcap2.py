import struct

data = open('/home/workspaces/onDemand/capture2.pcap', 'rb').read()
offset = 24
pkt_count = 0
submsg_ids = {}
# Track DATA sequence numbers
data_sns = []
# Track if we see ACKNACK
acknack_count = 0
# Track submessage details per packet
per_pkt = []

while offset < len(data):
    if offset + 16 > len(data): break
    ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', data[offset:offset+16])
    pkt_data = data[offset+16:offset+16+incl_len]
    offset += 16 + incl_len
    pkt_count += 1

    idx = pkt_data.find(b'RTPS')
    if idx < 0 or idx > 100:
        continue

    ptr = idx + 20
    end = len(pkt_data)
    pkt_submsgs = []

    while ptr + 4 <= end:
        sm_id = pkt_data[ptr]
        flags = pkt_data[ptr+1]
        le = bool(flags & 0x01)
        otn = struct.unpack('<H', pkt_data[ptr+2:ptr+4])[0] if le else struct.unpack('>H', pkt_data[ptr+2:ptr+4])[0]
        sm_end = end if otn == 0 else min(ptr + 4 + otn, end)
        submsg_ids[sm_id] = submsg_ids.get(sm_id, 0) + 1
        pkt_submsgs.append(sm_id)

        if sm_id == 0x15 and sm_end - ptr >= 24:  # DATA
            # Parse writerSN (8 bytes after readerEntityId + writerEntityId)
            # DATA layout: flags(1) + next(1) + octetsToNext(2) + extraFlags(2) + octetsToInlineQos(2) + readerId(4) + writerId(4) + writerSN(8)
            sn_ptr = ptr + 4 + 4 + 4 + 4  # skip header + extraFlags + otiq + readerId
            if sm_end - sn_ptr >= 8:
                # read writerEntityId first
                writer_eid = pkt_data[ptr+12:ptr+16]
                sn_high = struct.unpack('>i', pkt_data[sn_ptr:sn_ptr+4])[0]
                sn_low = struct.unpack('>I', pkt_data[sn_ptr+4:sn_ptr+8])[0]
                data_sns.append((writer_eid.hex(), sn_high, sn_low))

        if sm_id == 0x06:  # ACKNACK
            acknack_count += 1

        if otn == 0:
            break
        ptr += 4 + otn

    per_pkt.append(pkt_submsgs)

# Show first and last few packets' submessage composition
print(f'Total packets: {pkt_count}')
print(f'\nFirst 5 packets submsg composition:')
for i, subs in enumerate(per_pkt[:5]):
    names = {0x15:'DATA',0x16:'DATA_FRAG',0x07:'HB',0x06:'ACK',0x09:'ITS',0x0e:'IDST',0x0c:'ISRC'}
    sub_str = ' '.join(names.get(s, f'0x{s:02x}') for s in subs)
    print(f'  pkt[{i}]: {sub_str}')

print(f'\nLast 5 packets submsg composition:')
for i, subs in enumerate(per_pkt[-5:]):
    names = {0x15:'DATA',0x16:'DATA_FRAG',0x07:'HB',0x06:'ACK',0x09:'ITS',0x0e:'IDST',0x0c:'ISRC'}
    sub_str = ' '.join(names.get(s, f'0x{s:02x}') for s in subs)
    print(f'  pkt[{pkt_count-5+i}]: {sub_str}')

print(f'\nACKNACK messages: {acknack_count}')

# Show DATA sequence numbers grouped by writer
from collections import defaultdict
writer_sns = defaultdict(list)
for eid_hex, high, low in data_sns:
    writer_sns[eid_hex].append((high, low))

print(f'\nDATA sequence numbers by writer:')
for eid_hex, sns in writer_sns.items():
    first_sn = sns[0]
    last_sn = sns[-1]
    print(f'  writer {eid_hex}: count={len(sns)}, firstSN={first_sn[0]}:{first_sn[1]}, lastSN={last_sn[0]}:{last_sn[1]}')

# Show submsg ID distribution
print(f'\nSubmsg ID distribution:')
names = {0x15:'DATA',0x16:'DATA_FRAG',0x07:'HEARTBEAT',0x06:'ACKNACK',0x08:'GAP',0x09:'INFO_TS',0x0e:'INFO_DST',0x0c:'INFO_SRC'}
for k, v in sorted(submsg_ids.items()):
    name = names.get(k, '???')
    print(f'  0x{k:02x} {name:15s} {v}')
