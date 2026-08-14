"""OnDemand DDS Monitor v2.0 - Packet capture thread"""
import logging
import socket
import struct
import threading
import time
from typing import Optional, Set

from .rtps import (
    RTPS_MAGIC, ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER,
    ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER,
    ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER,
    parse_rtps_packet, parse_spdp_domain_id,
    extract_inline_qos_topic, extract_inline_qos_type,
    _read_inline_qos_at, _find_serialized_payload,
)
from .idl import decode_pub_table_define, decode_sub_table_register, decode_table_data_transfer
from .metrics import MetricsEngine

logger = logging.getLogger(__name__)


class PcapWorker:
    """Raw socket packet capture worker thread"""

    def __init__(self, iface: str, metrics: MetricsEngine,
                 port_filter: Optional[Set[int]] = None,
                 domain_filter: int = -1):
        self.iface = iface
        self.metrics = metrics
        self.port_filter = port_filter
        self.domain_filter = domain_filter
        self.running = threading.Event()
        self.sock: Optional[socket.socket] = None
        self._thread: Optional[threading.Thread] = None
        self._parsed_domain_id = -1

    def start(self):
        self.running.set()
        self._thread = threading.Thread(target=self._run, daemon=True, name="pcap-worker")
        self._thread.start()

    def stop(self):
        self.running.clear()
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        if self._thread:
            self._thread.join(timeout=3)

    def is_alive(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def _run(self):
        try:
            self.sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(0x0003))
            self.sock.bind((self.iface, 0))
            self.sock.settimeout(1.0)
            logger.info("pcap started iface=%s", self.iface)
        except PermissionError as e:
            logger.error("Permission denied: %s", e)
            return
        except OSError as e:
            logger.error("Bind error: %s", e)
            return

        while self.running.is_set():
            try:
                data, _ = self.sock.recvfrom(65535)
                self._process(data)
            except socket.timeout:
                continue
            except Exception:
                pass

    def _process(self, data: bytes):
        if len(data) < 42:
            return
        etype = struct.unpack("!H", data[12:14])[0]
        if etype != 0x0800:
            return
        ihl = (data[14] & 0x0F) * 4
        if data[23] != 17:
            return
        udp_start = 14 + ihl
        if len(data) < udp_start + 8:
            return

        sport, dport = struct.unpack("!HH", data[udp_start:udp_start + 4])
        udp_len = struct.unpack("!H", data[udp_start + 4:udp_start + 6])[0]
        payload = data[udp_start + 8:udp_start + udp_len]

        if self.port_filter:
            if sport not in self.port_filter and dport not in self.port_filter:
                return

        pkt_len = len(payload)
        self.metrics.on_packet(pkt_len, sport, dport)

        if pkt_len < 20 or payload[:4] != RTPS_MAGIC:
            return
        self.metrics.on_rtps()

        result = parse_rtps_packet(payload)
        if not result.get("valid"):
            return

        for submsg in result["submsgs"]:
            sid = submsg["id"]
            wid = submsg["writer_id"]
            length = submsg["length"]
            is_be = submsg["is_be"]
            offset = submsg["offset"]
            body_offset = submsg["body_offset"]

            self.metrics.on_submsg(sid, wid, length)

            if sid in (0x15, 0x16):
                topic_name = None
                for tname, tinfo in self.metrics.topics.items():
                    if tinfo.writer_id == wid:
                        topic_name = tname
                        break

                # SPDP: parse domain ID
                if wid == ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
                    did = parse_spdp_domain_id(payload, offset, length, is_be)
                    if did >= 0:
                        self._parsed_domain_id = did
                        guid_prefix = payload[8:20] if len(payload) >= 20 else b""
                        self.metrics.on_domain_discovered(did, guid_prefix, sport, dport, pkt_len)
                        logger.debug("SPDP domain_id=%d", did)

                # Domain filter
                current_domain = self._parsed_domain_id if self._parsed_domain_id >= 0 else -1
                if self.domain_filter >= 0 and current_domain != self.domain_filter:
                    continue

                if topic_name:
                    self.metrics.on_topic_packet(topic_name, length)

                # SEDP: discover topics
                if wid in (ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER,
                           ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER):
                    self._parse_sedp(payload, offset, length, is_be, wid)

                # GAP
                if sid == 0x08:
                    self.metrics.on_gap_event(wid)
                # FRAG
                if sid == 0x16:
                    self.metrics.on_frag_event(wid)

                # Raw packet record
                msg_type = ("SPDP" if wid == "000100c2" else
                    "SEDP-Pub" if wid == "000003c2" else
                    "SEDP-Sub" if wid == "000004c2" else
                    f"Data-B{int(wid[:6],16)-3}" if wid.endswith("03") else "Unknown")

                self.metrics.rtps_packets.append({
                    "time": time.time(), "sport": sport, "dport": dport,
                    "domain": current_domain, "wid": wid, "type": msg_type,
                    "topic": topic_name or "", "size": length,
                })

                # SN tracking (DATA only)
                if sid == 0x15:
                    self._extract_sn(payload, body_offset, is_be, wid)
                    self._decode_data_payload(payload, offset, length, is_be, wid)

    def _parse_sedp(self, payload, submsg_offset, submsg_len, is_be, wid_hex):
        """Parse SEDP DATA submessage to extract topic name from inline QoS."""
        try:
            body = submsg_offset + 4
            submsg_end = submsg_offset + 4 + submsg_len
            
            # Correct offset: inline QoS starts at body + 4 + octets_to_inline
            inline_start = _read_inline_qos_at(payload, body, is_be)
            
            if inline_start >= submsg_end or inline_start >= len(payload):
                return
            
            topic_name = extract_inline_qos_topic(payload, inline_start, submsg_end, is_be)
            type_name = extract_inline_qos_type(payload, inline_start, submsg_end, is_be)
            
            if topic_name:
                self.metrics.on_topic_discover(topic_name, type_name, wid_hex)
                logger.debug("SEDP discovered: %s -> %s", topic_name, wid_hex)
        except Exception as e:
            logger.debug("SEDP parse error: %s", e)

    def _extract_sn(self, payload, body_offset, is_be, wid_hex):
        """Extract sequence number from DATA submessage body.
        
        Body layout: extraFlags(2) + octets(2) + readerId(4) + writerId(4) + SN(8)
        SN starts at body + 12
        """
        try:
            sn_offset = body_offset + 12
            if sn_offset + 8 > len(payload):
                return
            fmt = ">ii" if is_be else "<ii"
            sn_high, sn_low = struct.unpack_from(fmt, payload, sn_offset)
            if sn_high == -1 and sn_low == -1:
                return
            sn = sn_low if sn_high == 0 else ((sn_high << 32) | (sn_low & 0xFFFFFFFF))
            if sn > 0:
                self.metrics.on_sn_update(wid_hex, sn)
        except Exception:
            pass

    def _decode_data_payload(self, payload, submsg_offset, submsg_len, is_be, wid_hex):
        """Decode IDL payload from DATA submessage."""
        try:
            body = submsg_offset + 4
            submsg_end = submsg_offset + 4 + submsg_len
            
            # Find serialized payload (after inline QoS + sentinel)
            payload_start = _find_serialized_payload(payload, body, submsg_end, is_be)
            if payload_start is None or payload_start >= submsg_end:
                return
            
            # SEDP-Pub -> PubTableDefine
            if wid_hex == "000003c2":
                decoded = decode_pub_table_define(payload, payload_start, is_be)
                if decoded:
                    self.metrics.on_pub_table_define(decoded)
            
            # SEDP-Sub -> SubTableRegister
            elif wid_hex == "000004c2":
                decoded = decode_sub_table_register(payload, payload_start, is_be)
                if decoded:
                    self.metrics.on_sub_register(decoded)
            
            # User writers -> TableDataTransfer
            elif wid_hex.endswith("03"):
                decoded = decode_table_data_transfer(payload, payload_start, is_be)
                if decoded:
                    ekey = int(wid_hex[:6], 16)
                    bucket = ekey - 3
                    self.metrics.data_transfers.append({
                        "time": time.time(), "bucket": bucket, "data": decoded,
                    })
        except Exception as e:
            logger.debug("decode payload error: %s", e)