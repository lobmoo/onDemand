"""
OnDemand DDS Monitor - RTPS 协议解析器

解析 RTPS 2.x 子消息: INFO_TS, DATA, HEARTBEAT, ACKNACK, GAP, INFO_DST
从 DATA 子消息的 inline QoS 中提取 topic name。
"""

import struct
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

# ── RTPS 子消息 ID ──
SUBMSG_ID_PAD = 0x01
SUBMSG_ID_ACKNACK = 0x06
SUBMSG_ID_HEARTBEAT = 0x07
SUBMSG_ID_GAP = 0x08
SUBMSG_ID_INFO_TS = 0x09
SUBMSG_ID_INFO_DST = 0x0e
SUBMSG_ID_DATA = 0x15
SUBMSG_ID_DATA_FRAG = 0x16

# ── QoS PID ──
PID_TOPIC_NAME = 0x0005
PID_SENTINEL = 0x0001
PID_KEY_HASH = 0x0070
PID_STATUS_INFO = 0x0071

# ── RTPS header ──
RTPS_MAGIC = b"RTPS"
RTPS_HEADER_SIZE = 20

# ── Entity ID ──
ENTITYID_PARTICIPANT = 0x000001c1
ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER = 0x000003c2
ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER = 0x000004c2


@dataclass
class RTPSHeader:
    """RTPS 包头"""
    protocol_version: Tuple[int, int]  # (major, minor)
    vendor_id: int
    guid_prefix: bytes  # 12 bytes


@dataclass
class SubMessage:
    """RTPS 子消息"""
    submsg_id: int
    flags: int
    length: int
    payload: bytes  # 子消息体 (不含 header)
    is_big_endian: bool


@dataclass
class DataSubMessage:
    """DATA 子消息解析结果"""
    reader_id: bytes  # 4 bytes
    writer_id: bytes  # 4 bytes
    sequence_number: int
    topic_name: Optional[str]  # 从 inline QoS 提取
    serialized_data: Optional[bytes]  # 序列化数据 (不含 encapsulation header)


@dataclass
class HeartbeatSubMessage:
    """HEARTBEAT 子消息"""
    reader_id: bytes
    writer_id: bytes
    first_sn: int  # first available sequence number
    last_sn: int  # last available sequence number
    count: int  # heartbeat count


@dataclass
class AckNackSubMessage:
    """ACKNACK 子消息"""
    reader_id: bytes
    writer_id: bytes
    reader_sn_state: int  # 下一个期望的 SN
    count: int


@dataclass
class GapSubMessage:
    """GAP 子消息"""
    reader_id: bytes
    writer_id: bytes
    gap_start: int  # gap 起始 SN
    gap_list: List[int]  # gap 中的 SN 列表 (简化)


class RTPSParser:
    """RTPS 包解析器"""

    @staticmethod
    def parse_header(data: bytes) -> Optional[RTPSHeader]:
        """解析 RTPS 包头"""
        if len(data) < RTPS_HEADER_SIZE:
            return None
        if data[:4] != RTPS_MAGIC:
            return None
        major = data[4]
        minor = data[5]
        vendor_id = struct.unpack_from("!H", data, 6)[0]
        guid_prefix = data[8:20]
        return RTPSHeader(
            protocol_version=(major, minor), vendor_id=vendor_id, guid_prefix=guid_prefix
        )

    @staticmethod
    def parse_submessages(data: bytes) -> List[SubMessage]:
        """解析所有子消息"""
        msgs = []
        offset = RTPS_HEADER_SIZE
        while offset + 4 <= len(data):
            submsg_id = data[offset]
            flags = data[offset + 1]
            is_big_endian = not (flags & 0x01)  # bit 0 = endianness flag (0=BE, 1=LE)

            if is_big_endian:
                length = struct.unpack_from("!H", data, offset + 2)[0]
            else:
                length = struct.unpack_from("<H", data, offset + 2)[0]

            payload_start = offset + 4
            if length == 0:
                # 子消息延伸到包尾
                payload = data[payload_start:]
                msgs.append(
                    SubMessage(
                        submsg_id=submsg_id,
                        flags=flags,
                        length=len(payload),
                        payload=payload,
                        is_big_endian=is_big_endian,
                    )
                )
                break
            else:
                if payload_start + length > len(data):
                    break
                payload = data[payload_start : payload_start + length]
                msgs.append(
                    SubMessage(
                        submsg_id=submsg_id,
                        flags=flags,
                        length=length,
                        payload=payload,
                        is_big_endian=is_big_endian,
                    )
                )
                offset = payload_start + length
        return msgs

    @staticmethod
    def parse_data_submessage(msg: SubMessage) -> Optional[DataSubMessage]:
        """解析 DATA 子消息"""
        payload = msg.payload
        if len(payload) < 24:  # minimum: extraFlags(2) + octetsToInlineQoS(2) + readerId(4) + writerId(4) + SN(8) + inlineQoS header
            return None

        if msg.is_big_endian:
            fmt = "!HH4s4sII"
        else:
            fmt = "<HH4s4sII"

        extra_flags, octets_to_inline_qos, reader_id, writer_id, sn_high, sn_low = (
            struct.unpack_from(fmt, payload, 0)
        )

        # Sequence number: high 32 bits + low 32 bits
        sequence_number = (sn_high << 32) | sn_low
        if sn_high == 0xFFFFFFFF:  # invalid SN
            sequence_number = -1

        # Inline QoS starts at offset 16 (after extraFlags + octetsToInlineQoS + readerId + writerId + SN)
        inline_qos_offset = 16
        topic_name = None
        serialized_data = None

        # 解析 inline QoS parameters
        if inline_qos_offset < len(payload):
            topic_name, data_offset = RTPSParser._parse_inline_qos(
                payload[inline_qos_offset:], msg.is_big_endian
            )
            if data_offset > 0:
                abs_offset = inline_qos_offset + data_offset
                if abs_offset < len(payload):
                    # 跳过 encapsulation header (4 bytes)
                    serialized_data = payload[abs_offset + 4 :]

        return DataSubMessage(
            reader_id=reader_id,
            writer_id=writer_id,
            sequence_number=sequence_number,
            topic_name=topic_name,
            serialized_data=serialized_data,
        )

    @staticmethod
    def _parse_inline_qos(
        data: bytes, is_big_endian: bool
    ) -> Tuple[Optional[str], int]:
        """解析 inline QoS 参数列表，返回 (topic_name, payload_offset)"""
        topic_name = None
        offset = 0
        endian = "!" if is_big_endian else "<"

        while offset + 4 <= len(data):
            pid = struct.unpack_from(f"{endian}H", data, offset)[0]
            length = struct.unpack_from(f"{endian}H", data, offset + 2)[0]

            if pid == PID_SENTINEL:
                # SENTINEL 后面是 padding (4 bytes alignment) 然后是 serialized payload
                offset += 4  # skip PID + length
                # align to 4 bytes
                offset = (offset + 3) & ~3
                return topic_name, offset

            if pid == PID_TOPIC_NAME and length > 0:
                # topic name: null-terminated string + padding
                name_bytes = data[offset + 4 : offset + 4 + length]
                # 去掉 null terminator 和 padding
                null_pos = name_bytes.find(b"\x00")
                if null_pos >= 0:
                    name_bytes = name_bytes[:null_pos]
                try:
                    topic_name = name_bytes.decode("utf-8", errors="replace")
                except Exception:
                    topic_name = None

            if length == 0:
                break
            offset += 4 + length
            # align to 4 bytes
            offset = (offset + 3) & ~3

        return topic_name, offset

    @staticmethod
    def parse_heartbeat(msg: SubMessage) -> Optional[HeartbeatSubMessage]:
        """解析 HEARTBEAT 子消息"""
        payload = msg.payload
        if len(payload) < 28:
            return None
        if msg.is_big_endian:
            fmt = "!4s4sIIi"
        else:
            fmt = "<4s4sIIi"
        reader_id, writer_id, first_high, first_low, last_high, last_low, count = struct.unpack_from(
            fmt, payload, 0
        )
        # 修正: HEARTBEAT 结构是 readerId(4) + writerId(4) + firstSN(8) + lastSN(8) + count(4) = 28
        # 但上面只 unpack 了 24 bytes, 需要修正
        if msg.is_big_endian:
            fmt2 = "!4s4sIIIIi"
        else:
            fmt2 = "<4s4sIIIIi"
        reader_id, writer_id, first_sn_high, first_sn_low, last_sn_high, last_sn_low, count = (
            struct.unpack_from(fmt2, payload, 0)
        )
        first_sn = (first_sn_high << 32) | first_sn_low
        last_sn = (last_sn_high << 32) | last_sn_low
        return HeartbeatSubMessage(
            reader_id=reader_id,
            writer_id=writer_id,
            first_sn=first_sn,
            last_sn=last_sn,
            count=count,
        )

    @staticmethod
    def parse_acknack(msg: SubMessage) -> Optional[AckNackSubMessage]:
        """解析 ACKNACK 子消息"""
        payload = msg.payload
        if len(payload) < 16:
            return None
        if msg.is_big_endian:
            fmt = "!4s4sIi"
        else:
            fmt = "<4s4sIi"
        reader_id, writer_id, reader_sn_state, count = struct.unpack_from(fmt, payload, 0)
        return AckNackSubMessage(
            reader_id=reader_id,
            writer_id=writer_id,
            reader_sn_state=reader_sn_state,
            count=count,
        )

    @staticmethod
    def parse_gap(msg: SubMessage) -> Optional[GapSubMessage]:
        """解析 GAP 子消息 (简化)"""
        payload = msg.payload
        if len(payload) < 24:
            return None
        if msg.is_big_endian:
            fmt = "!4s4sII"
        else:
            fmt = "<4s4sII"
        reader_id, writer_id, gap_start_high, gap_start_low = struct.unpack_from(fmt, payload, 0)
        gap_start = (gap_start_high << 32) | gap_start_low
        # Gap list 简化处理：跳过 bitmap
        return GapSubMessage(
            reader_id=reader_id,
            writer_id=writer_id,
            gap_start=gap_start,
            gap_list=[],  # 简化
        )

    @classmethod
    def parse_packet(cls, data: bytes) -> Dict:
        """
        解析一个完整的 RTPS 包，返回解析结果摘要。

        Returns:
            {
                "header": Optional[RTPSHeader],
                "submsg_types": [int, ...],  # 子消息类型列表
                "topics": [str, ...],  # 从 DATA 子消息提取的 topic
                "heartbeats": [HeartbeatSubMessage, ...],
                "acknacks": [AckNackSubMessage, ...],
                "gaps": [GapSubMessage, ...],
                "data_msgs": [DataSubMessage, ...],
            }
        """
        header = cls.parse_header(data)
        if header is None:
            return {"header": None}

        submsgs = cls.parse_submessages(data)
        result = {
            "header": header,
            "submsg_types": [],
            "topics": [],
            "heartbeats": [],
            "acknacks": [],
            "gaps": [],
            "data_msgs": [],
        }

        for msg in submsgs:
            result["submsg_types"].append(msg.submsg_id)

            if msg.submsg_id == SUBMSG_ID_DATA:
                data_msg = cls.parse_data_submessage(msg)
                if data_msg:
                    result["data_msgs"].append(data_msg)
                    if data_msg.topic_name:
                        result["topics"].append(data_msg.topic_name)

            elif msg.submsg_id == SUBMSG_ID_HEARTBEAT:
                hb = cls.parse_heartbeat(msg)
                if hb:
                    result["heartbeats"].append(hb)

            elif msg.submsg_id == SUBMSG_ID_ACKNACK:
                ack = cls.parse_acknack(msg)
                if ack:
                    result["acknacks"].append(ack)

            elif msg.submsg_id == SUBMSG_ID_GAP:
                gap = cls.parse_gap(msg)
                if gap:
                    result["gaps"].append(gap)

        return result
