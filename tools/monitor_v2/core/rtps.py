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
            length = len(payload) - offset - 4
            if length <= 0:
                break

        body_offset = offset + 4
        writer_id = ""
        if sid in (0x15, 0x16) and length >= 12:
            writer_id = payload[body_offset + 8:body_offset + 12].hex()
        elif sid in (0x06, 0x07, 0x08) and length >= 8:
            writer_id = payload[body_offset + 4:body_offset + 8].hex()

        submsgs.append({
            "id": sid, "writer_id": writer_id, "length": length,
            "is_be": is_be, "offset": offset, "body_offset": body_offset,
        })
        offset = body_offset + length

    return {"valid": True, "submsgs": submsgs}


def _read_inline_qos_at(payload, body_offset, is_be):
    """Calculate the correct offset to inline QoS start.
    
    DATA submessage body layout:
      extraFlags(2) + octetsToInlineQoS(2) + readerId(4) + writerId(4) + SN(8)
    
    octetsToInlineQoS = number of octets from END of that field to inline QoS start.
    So inline QoS starts at: body + 4 + octetsToInlineQoS
    """
    octets_to_inline = struct.unpack_from(">H" if is_be else "<H", payload, body_offset + 2)[0]
    # Correct: inline QoS starts after extraFlags(2)+octets(2) + octets_to_inline
    inline_start = body_offset + 4 + octets_to_inline
    return inline_start


def _find_serialized_payload(payload, body_offset, submsg_end, is_be):
    """Find the serialized payload after inline QoS.
    
    Inline QoS ends at PID_SENTINEL. After that may be padding, then payload.
    """
    inline_start = _read_inline_qos_at(payload, body_offset, is_be)
    fmt = ">H" if is_be else "<H"
    offset = inline_start

    while offset + 4 <= submsg_end:
        pid = struct.unpack_from(fmt, payload, offset)[0]
        plen = struct.unpack_from(fmt, payload, offset + 2)[0]
        if pid == PID_SENTINEL:
            offset += 4
            # align to 4
            r = offset % 4
            if r:
                offset += 4 - r
            return offset
        if plen == 0:
            break
        offset += 4 + plen
        r = offset % 4
        if r:
            offset += 4 - r
    return None


def extract_inline_qos_topic(buf, offset, end, is_be):
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


def extract_inline_qos_type(buf, offset, end, is_be):
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


def parse_spdp_domain_id(payload, submsg_offset, submsg_len, is_be):
    """Parse domain ID from SPDP DATA submessage.
    
    Strategy:
    1. Try inline QoS first (PID_DOMAIN_ID)
    2. If not found, parse serialized payload (ParticipantBuiltinTopicData)
    
    Returns -1 on failure.
    """
    try:
        body = submsg_offset + 4
        submsg_end = submsg_offset + 4 + submsg_len
        
        # --- Strategy 1: Try inline QoS ---
        inline_start = _read_inline_qos_at(payload, body, is_be)
        
        if inline_start + 4 < submsg_end and inline_start < len(payload):
            # Read CDR header for actual byte order
            cdr_kind = struct.unpack_from(">H", payload, inline_start)[0]
            cdr_be = cdr_kind in (0x0002, 0x0000)  # PL_CDR_BE / CDR_BE
            
            fmt = ">H" if cdr_be else "<H"
            fmt_i = ">I" if cdr_be else "<I"
            
            offset = inline_start + 4  # skip CDR header
            search_end = min(submsg_end, len(payload))
            
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
        
        # --- Strategy 2: Parse serialized payload ---
        payload_start = _find_serialized_payload(payload, body, submsg_end, is_be)
        if payload_start is None or payload_start >= submsg_end:
            return -1
        
        # The serialized payload is a CDR parameter list (PL_CDR)
        # Read CDR header
        if payload_start + 4 > len(payload):
            return -1
        cdr_kind = struct.unpack_from(">H", payload, payload_start)[0]
        cdr_be = cdr_kind in (0x0002, 0x0000)
        
        fmt = ">H" if cdr_be else "<H"
        fmt_i = ">i" if cdr_be else "<i"  # domainId is signed int32
        
        offset = payload_start + 4
        search_end = min(submsg_end, len(payload))
        
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
    except Exception as e:
        return -1