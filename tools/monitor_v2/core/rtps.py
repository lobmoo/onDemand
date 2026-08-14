"""OnDemand DDS Monitor v2.0 - RTPS protocol parser"""
import struct
from typing import Dict, List, Optional

# RTPS constants
RTPS_MAGIC = b"RTPS"
RTPS_HEADER_SIZE = 20

# Submessage IDs
SUBMSG_ID_PAD = 0x01
SUBMSG_ID_ACKNACK = 0x06
SUBMSG_ID_HEARTBEAT = 0x07
SUBMSG_ID_GAP = 0x08
SUBMSG_ID_INFO_TS = 0x09
SUBMSG_ID_INFO_DST = 0x0e
SUBMSG_ID_DATA = 0x15
SUBMSG_ID_DATA_FRAG = 0x16
SUBMSG_ID_VENDOR = 0x80

SUBMSG_NAMES = {
    0x01: "PAD", 0x06: "ACKNACK", 0x07: "HEARTBEAT",
    0x08: "GAP", 0x09: "INFO_TS", 0x0e: "INFO_DST",
    0x15: "DATA", 0x16: "DATA_FRAG", 0x80: "VENDOR",
}

# QoS PIDs
PID_TOPIC_NAME = 0x0005
PID_TYPE_NAME = 0x0007
PID_DOMAIN_ID = 0x000F
PID_SENTINEL = 0x0001
PID_KEY_HASH = 0x0070
PID_STATUS_INFO = 0x0071

# Entity IDs
ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER = "000100c2"
ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER = "000003c2"
ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER = "000004c2"


def parse_rtps_packet(payload: bytes) -> Dict:
    """Parse a complete RTPS packet, return submessage list."""
    if len(payload) < RTPS_HEADER_SIZE:
        return {"valid": False, "submsgs": []}
    if payload[:4] != RTPS_MAGIC:
        return {"valid": False, "submsgs": []}

    submsgs = []
    offset = RTPS_HEADER_SIZE
    while offset + 4 <= len(payload):
        sid = payload[offset]
        flags = payload[offset + 1]
        is_be = not (flags & 0x01)

        fmt = "!H" if is_be else "<H"
        length = struct.unpack_from(fmt, payload, offset + 2)[0]

        if length == 0:
            # extends to end of packet
            length = len(payload) - offset - 4
            if length <= 0:
                break

        body_offset = offset + 4
        writer_id = ""
        if sid in (0x15, 0x16) and length >= 12:
            # DATA: extraFlags(2) + octetsToInlineQoS(2) + readerId(4) + writerId(4)
            writer_id = payload[body_offset + 8:body_offset + 12].hex()
        elif sid in (0x06, 0x07, 0x08) and length >= 8:
            # ACKNACK/HEARTBEAT/GAP: readerId(4) + writerId(4)
            writer_id = payload[body_offset + 4:body_offset + 8].hex()

        submsgs.append({
            "id": sid,
            "writer_id": writer_id,
            "length": length,
            "is_be": is_be,
            "offset": offset,
            "body_offset": body_offset,
        })

        offset = body_offset + length

    return {"valid": True, "submsgs": submsgs}


def extract_inline_qos_topic(buf: bytes, offset: int, end: int, is_be: bool) -> Optional[str]:
    """Extract PID_TOPIC_NAME from inline QoS parameter list."""
    fmt = ">H" if is_be else "<H"
    while offset + 4 <= end:
        pid = struct.unpack_from(fmt, buf, offset)[0]
        plen = struct.unpack_from(fmt, buf, offset + 2)[0]
        if pid == PID_SENTINEL:
            break
        if pid == PID_TOPIC_NAME and plen > 4:
            from .cdr import cdr_read_string
            s, _ = cdr_read_string(buf, offset + 4, is_be)
            return s if s else None
        if plen == 0:
            break
        offset += 4 + plen
        if plen % 4:
            offset += 4 - (plen % 4)
    return None


def extract_inline_qos_type(buf: bytes, offset: int, end: int, is_be: bool) -> Optional[str]:
    """Extract PID_TYPE_NAME from inline QoS parameter list."""
    fmt = ">H" if is_be else "<H"
    while offset + 4 <= end:
        pid = struct.unpack_from(fmt, buf, offset)[0]
        plen = struct.unpack_from(fmt, buf, offset + 2)[0]
        if pid == PID_SENTINEL:
            break
        if pid == PID_TYPE_NAME and plen > 4:
            from .cdr import cdr_read_string
            s, _ = cdr_read_string(buf, offset + 4, is_be)
            return s if s else None
        if plen == 0:
            break
        offset += 4 + plen
        if plen % 4:
            offset += 4 - (plen % 4)
    return None


def parse_spdp_domain_id(payload: bytes, submsg_offset: int, submsg_len: int, is_be: bool) -> int:
    """Parse domain ID from SPDP DATA submessage. Returns -1 on failure."""
    try:
        body = submsg_offset + 4
        octets_to_inline = struct.unpack_from(">H" if is_be else "<H", payload, body + 2)[0]
        inline_start = body + octets_to_inline
        inline_end = submsg_offset + 4 + submsg_len

        if inline_start + 4 >= inline_end or inline_start >= len(payload):
            return -1

        # Read CDR header for actual byte order
        cdr_kind = struct.unpack_from(">H", payload, inline_start)[0]
        if cdr_kind in (0x0002, 0x0000):  # PL_CDR_BE / CDR_BE
            cdr_be = True
        else:  # PL_CDR_LE / CDR_LE
            cdr_be = False

        fmt = ">H" if cdr_be else "<H"
        fmt_i = ">I" if cdr_be else "<I"

        offset = inline_start + 4  # skip CDR header
        search_end = min(inline_end, len(payload))

        while offset + 4 <= search_end:
            pid = struct.unpack_from(fmt, payload, offset)[0]
            plen = struct.unpack_from(fmt, payload, offset + 2)[0]
            if pid == PID_SENTINEL:
                break
            if pid == PID_DOMAIN_ID and plen >= 4:
                return struct.unpack_from(fmt_i, payload, offset + 4)[0]
            if plen == 0:
                break
            offset += 4 + plen
            r = offset % 4
            if r:
                offset += 4 - r
        return -1
    except Exception:
        return -1