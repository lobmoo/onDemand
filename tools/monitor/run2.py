#!/usr/bin/env python3
"""
OnDemand DDS Monitor - 基于 writerId 映射的抓包监控
用法:
  sudo python3 run2.py -i enx0826ae3a20b7
  sudo python3 run2.py -i lo --debug  # 抓所有UDP
"""
import os
import sys
import argparse
import curses
import logging
import socket
import struct
import threading
import time
from collections import defaultdict, deque


# ═══════════════════════════════════════════════════════════════
#  CDR 辅助函数 (用于解析 SEDP discovery 和 IDL 消息)
# ═══════════════════════════════════════════════════════════════
def cdr_read_string(buf, offset, is_be):
    """从 CDR 缓冲区读取一个 string (4字节长度 + 内容 + 对齐padding)"""
    if offset + 4 > len(buf):
        return None, offset
    slen = struct.unpack_from(">I" if is_be else "<I", buf, offset)[0]
    offset += 4
    if slen == 0 or offset + slen > len(buf):
        return "", offset
    s = buf[offset:offset + slen - 1].decode("utf-8", errors="replace")
    offset += slen
    # align to 4
    if offset % 4:
        offset += 4 - (offset % 4)
    return s, offset


def extract_inline_qos_topic(buf, offset, end, is_be):
    """从 inline QoS 参数列表中提取 PID_TOPIC_NAME (0x0005)"""
    while offset + 4 <= end:
        pid = struct.unpack_from(">H" if is_be else "<H", buf, offset)[0]
        plen = struct.unpack_from(">H" if is_be else "<H", buf, offset + 2)[0]
        if pid == 0x0001:  # PID_SENTINEL
            break
        if pid == 0x0005 and plen > 4:  # PID_TOPIC_NAME
            s, _ = cdr_read_string(buf, offset + 4, is_be)
            return s
        offset += 4 + plen
        if plen % 4:
            offset += 4 - (plen % 4)
    return None


def extract_inline_qos_type(buf, offset, end, is_be):
    """从 inline QoS 参数列表中提取 PID_TYPE_NAME (0x0007)"""
    while offset + 4 <= end:
        pid = struct.unpack_from(">H" if is_be else "<H", buf, offset)[0]
        plen = struct.unpack_from(">H" if is_be else "<H", buf, offset + 2)[0]
        if pid == 0x0001:  # PID_SENTINEL
            break
        if pid == 0x0007 and plen > 4:  # PID_TYPE_NAME
            s, _ = cdr_read_string(buf, offset + 4, is_be)
            return s
        offset += 4 + plen
        if plen % 4:
            offset += 4 - (plen % 4)
    return None


# ═══════════════════════════════════════════════════════════════
#  CDR Reader - 解码 IDL 序列化消息
# ═══════════════════════════════════════════════════════════════
class CDRReader:
    """CDR (Common Data Representation) 解码器，支持 CDR_LE / CDR_BE"""
    def __init__(self, buf, offset=0, is_be=False):
        self.buf = buf
        self.off = offset
        self.is_be = is_be

    def _fmt(self, code):
        return (">" if self.is_be else "<") + code

    def align(self, n):
        r = self.off % n
        if r:
            self.off += n - r

    def read_i32(self):
        self.align(4)
        v = struct.unpack_from(self._fmt("i"), self.buf, self.off)[0]
        self.off += 4
        return v

    def read_u32(self):
        self.align(4)
        v = struct.unpack_from(self._fmt("I"), self.buf, self.off)[0]
        self.off += 4
        return v

    def read_i64(self):
        self.align(8)
        v = struct.unpack_from(self._fmt("q"), self.buf, self.off)[0]
        self.off += 8
        return v

    def read_u64(self):
        self.align(8)
        v = struct.unpack_from(self._fmt("Q"), self.buf, self.off)[0]
        self.off += 8
        return v

    def read_bool(self):
        v = struct.unpack_from("B", self.buf, self.off)[0]
        self.off += 1
        return v != 0

    def read_u8(self):
        v = struct.unpack_from("B", self.buf, self.off)[0]
        self.off += 1
        return v

    def read_string(self):
        slen = self.read_u32()
        if slen == 0:
            return ""
        s = self.buf[self.off:self.off + slen - 1].decode("utf-8", errors="replace")
        self.off += slen
        self.align(4)
        return s

    def read_bytes(self, n):
        v = self.buf[self.off:self.off + n]
        self.off += n
        return v

    def remaining(self):
        return len(self.buf) - self.off

    def skip(self, n):
        self.off += n


def decode_pub_table_define(buf, offset, is_be):
    """解码 PubTableDefine 消息
    返回: {"name": str, "nodeName": str, "description": str, "vars": [{"id": int, "name": str, "size": int, ...}, ...]}
    """
    try:
        r = CDRReader(buf, offset, is_be)
        table_name = r.read_string()
        node_name = r.read_string()
        description = r.read_string()
        # sequence<PubTableVarDefine>
        count = r.read_u32()
        logging.debug("PubTableDefine: table=%s node=%s count=%d", table_name, node_name, count)
        vars_list = []
        for _ in range(count):
            var_id = r.read_u64()  # uint64 id (varHash)
            # VarRequest union: discriminator DEFINE_TYPE (i32)
            disc = r.read_i32()
            if disc == 0:  # VAR_DEFINE -> Define
                var_name = r.read_string()
                var_size = r.read_i32()
                model_name = r.read_string()
                model_version = r.read_string()
                var_desc = r.read_string()
                var_node = r.read_string()
                is_readonly = r.read_bool()
                # VarPublishMask (bitmask, u32) - 需要先对齐到4
                r.align(4)
                publish_mask = r.read_u32()
                vars_list.append({
                    "id": var_id,
                    "name": var_name,
                    "size": var_size,
                    "nodeName": var_node,
                    "model": model_name,
                })
            elif disc == 1:  # VAR_UNDEFINE -> Undefine
                var_name = r.read_string()
                var_node = r.read_string()
                vars_list.append({"id": var_id, "name": var_name, "removed": True})
            else:
                logging.debug("PubTableDefine: unknown discriminator %d", disc)
                break
        return {
            "name": table_name,
            "nodeName": node_name,
            "description": description,
            "vars": vars_list,
            "var_count": len(vars_list),
        }
    except Exception as e:
        logging.debug("decode_pub_table_define error: %s", e)
        return None


def decode_sub_table_register(buf, offset, is_be):
    """解码 SubTableRegister 消息
    返回: {"msgType": int, "tableName": str, "nodeName": str, "user": str, "vars": [{"name": str, "freq": str}, ...]}
    """
    try:
        r = CDRReader(buf, offset, is_be)
        msg_type = r.read_i32()  # MSGTYPE enum
        # sequence<NamedValue> varFreqs
        count = r.read_u32()
        vars_list = []
        for _ in range(count):
            nv_name = r.read_string()   # varName
            nv_value = r.read_string()  # frequency (string)
            vars_list.append({"name": nv_name, "freq": nv_value})
        table_name = r.read_string()
        node_name = r.read_string()
        user = r.read_string()
        return {
            "msgType": msg_type,
            "tableName": table_name,
            "nodeName": node_name,
            "user": user,
            "vars": vars_list,
            "var_count": len(vars_list),
        }
    except Exception as e:
        logging.debug("decode_sub_table_register error: %s", e)
        return None


def decode_table_data_transfer(buf, offset, is_be):
    """解码 TableDataTransfer 消息 (简化版，只提取 mask 和 varData 数量)
    返回: {"var_count": int, "blob_type": int, "ts_sec": int, "ts_nsec": int}
    """
    try:
        r = CDRReader(buf, offset, is_be)
        # DT_BLOB mask: sequence<octet>
        mask_len = r.read_u32()
        mask_bytes = r.read_bytes(mask_len)
        r.align(4)
        # Timespec: i32 tv_sec + u64 tv_nsec
        tv_sec = r.read_i32()
        tv_nsec = r.read_u64()
        # sequence<DT_BLOB> varData
        var_data_count = r.read_u32()
        # blobType enum (i32)
        blob_type = r.read_i32()
        return {
            "var_count": var_data_count,
            "blob_type": blob_type,
            "ts_sec": tv_sec,
            "ts_nsec": tv_nsec,
            "mask_len": mask_len,
        }
    except Exception as e:
        logging.debug("decode_table_data_transfer error: %s", e)
        return None


# OnDemand IDL topic 名称和类型映射
KNOWN_TOPICS = {
    "dsf/sys/var/tableDefine": "DSF::Var::PubTableDefine",
    "dsf/message/commandRequest/subTableRegister": "DSF::Message::SubTableRegister",
}
for i in range(20):
    KNOWN_TOPICS[f"dsf/var/data/transfer/bucket_{i}"] = "DSF::Var::TableDataTransfer"

RTPS_MAGIC = b"RTPS"
SUBMSG_NAMES = {
    0x01: "PAD", 0x06: "ACKNACK", 0x07: "HEARTBEAT",
    0x08: "GAP", 0x09: "INFO_TS", 0x0e: "INFO_DST",
    0x15: "DATA", 0x16: "DATA_FRAG", 0x80: "VENDOR",
}

# ═══════════════════════════════════════════════════════════════
#  滑动窗口速率计
# ═══════════════════════════════════════════════════════════════
class RateMeter:
    def __init__(self, window=2.0):
        self._window = window
        self._events = deque()
        self._lock = threading.Lock()

    def add(self, ts=None, val=1.0):
        if ts is None:
            ts = time.time()
        with self._lock:
            self._events.append((ts, val))
            self._evict(ts)

    def _evict(self, now):
        cutoff = now - self._window
        while self._events and self._events[0][0] < cutoff:
            self._events.popleft()

    def rate(self):
        with self._lock:
            if len(self._events) < 2:
                return 0.0
            dt = self._events[-1][0] - self._events[0][0]
            return (len(self._events) - 1) / dt if dt > 0 else 0.0

    def count(self):
        with self._lock:
            return len(self._events)


# ═══════════════════════════════════════════════════════════════
#  统计引擎
# ═══════════════════════════════════════════════════════════════
class Stats:
    def __init__(self):
        self.lock = threading.Lock()
        self.t0 = time.time()

        # 包级别
        self.pkt_total = 0
        self.pkt_rtps = 0
        self.bytes_total = 0

        # 子消息计数
        self.submsg_counts = defaultdict(int)

        # DATA writer 统计: wid_hex -> {count, bytes, rate}
        self.data_writers = defaultdict(lambda: {"count": 0, "bytes": 0, "rate": RateMeter()})

        # SPDP/SEDP writer 统计
        self.spdp_count = 0
        self.sedp_pub_count = 0
        self.sedp_sub_count = 0

        # 端口统计
        self.port_counts = defaultdict(int)

        # 速率
        self.pkt_rate = RateMeter()
        self.byte_rate = RateMeter()
        self.data_rate = RateMeter()

        # 最近事件
        self.events = deque(maxlen=30)

        # DDS 层: SEDP 发现的 topic → writerId 映射
        self.topic_writers = {}  # topic_name -> {"wid": wid_hex, "type": type_name, "first_seen": ts, "count": int}
        self.topic_pkt_count = defaultdict(int)  # topic_name -> packet count
        self.topic_byte_count = defaultdict(int)  # topic_name -> byte count
        self.topic_rate = defaultdict(lambda: RateMeter())  # topic_name -> rate

        # DDS 层: 解码后的消息数据
        self.pub_table_defines = []  # PubTableDefine 消息列表 (最近N条)
        self.sub_registers = []      # SubTableRegister 消息列表 (最近N条)
        self.data_transfers = []     # TableDataTransfer 简要信息 (最近N条)
        self.known_vars = {}         # varHash -> {"name", "nodeName", "size", "bucket", "table"}
        self.known_subs = {}         # nodeName -> {"vars": [...], "tableName", "last_seen"}

    def on_packet(self, pkt_len, sport=0, dport=0):
        now = time.time()
        with self.lock:
            self.pkt_total += 1
            self.bytes_total += pkt_len
            self.pkt_rate.add(now)
            self.byte_rate.add(now, pkt_len)
            if dport in (7400, 7401):
                self.port_counts[dport] += 1
            elif sport in (7400, 7401):
                self.port_counts[sport] += 1

    def on_rtps(self):
        with self.lock:
            self.pkt_rtps += 1

    def on_submsg(self, sid, wid_hex=None, frag_len=0, port=0):
        now = time.time()
        with self.lock:
            self.submsg_counts[sid] += 1

            if sid == 0x15:  # DATA
                self.data_rate.add(now)
                if wid_hex:
                    w = self.data_writers[wid_hex]
                    w["count"] += 1
                    w["bytes"] += frag_len
                    w["rate"].add(now)
                    # 识别 SPDP vs SEDP
                    if wid_hex == "000100c2":
                        self.spdp_count += 1
                    elif wid_hex == "000003c2":
                        self.sedp_pub_count += 1
                    elif wid_hex == "000004c2":
                        self.sedp_sub_count += 1
                    # DDS 层: 如果已知该 writer 对应的 topic，更新 topic 统计
                    for tname, tinfo in self.topic_writers.items():
                        if tinfo["wid"] == wid_hex:
                            self.topic_pkt_count[tname] += 1
                            self.topic_byte_count[tname] += frag_len
                            self.topic_rate[tname].add(now)
                            tinfo["count"] += 1
                            break

            elif sid == 0x16:  # DATA_FRAG
                self.data_rate.add(now)
                if wid_hex:
                    w = self.data_writers[wid_hex]
                    w["count"] += 1
                    w["bytes"] += frag_len
                    w["rate"].add(now)
                    # DDS 层: topic 统计
                    for tname, tinfo in self.topic_writers.items():
                        if tinfo["wid"] == wid_hex:
                            self.topic_pkt_count[tname] += 1
                            self.topic_byte_count[tname] += frag_len
                            self.topic_rate[tname].add(now)
                            tinfo["count"] += 1
                            break

    def on_topic_discover(self, topic_name, type_name, wid_hex):
        """SEDP 发现: 记录 topic ↔ writer 映射"""
        now = time.time()
        with self.lock:
            if topic_name not in self.topic_writers:
                self.topic_writers[topic_name] = {
                    "wid": wid_hex,
                    "type": type_name or KNOWN_TOPICS.get(topic_name, ""),
                    "first_seen": now,
                    "count": 0,
                }
                self.events.append(f"[SEDP] {topic_name} -> {wid_hex}")
            else:
                # 更新 writer 映射 (可能 writer 重启了)
                self.topic_writers[topic_name]["wid"] = wid_hex

    def on_pub_table_define(self, decoded):
        """收到 PubTableDefine 消息"""
        now = time.time()
        with self.lock:
            self.pub_table_defines.append({"time": now, "data": decoded})
            if len(self.pub_table_defines) > 50:
                self.pub_table_defines.pop(0)
            # 更新 known_vars
            table = decoded.get("name", "")
            node = decoded.get("nodeName", "")
            for v in decoded.get("vars", []):
                vid = v.get("id", 0)
                if v.get("removed"):
                    self.known_vars.pop(vid, None)
                else:
                    bucket = vid % 20
                    self.known_vars[vid] = {
                        "name": v.get("name", ""),
                        "nodeName": node,
                        "size": v.get("size", 0),
                        "bucket": bucket,
                        "table": table,
                    }
            self.events.append(f"[DEFINE] {node}/{table}: {decoded.get('var_count', 0)} vars")

    def on_sub_register(self, decoded):
        """收到 SubTableRegister 消息"""
        now = time.time()
        with self.lock:
            self.sub_registers.append({"time": now, "data": decoded})
            if len(self.sub_registers) > 50:
                self.sub_registers.pop(0)
            node = decoded.get("nodeName", "")
            table = decoded.get("tableName", "")
            self.known_subs[node] = {
                "vars": decoded.get("vars", []),
                "tableName": table,
                "last_seen": now,
                "msgType": decoded.get("msgType", 0),
            }
            self.events.append(f"[SUB] {node} -> {table}: {decoded.get('var_count', 0)} vars")

    def on_data_transfer(self, bucket, decoded):
        """收到 TableDataTransfer 消息"""
        now = time.time()
        with self.lock:
            self.data_transfers.append({"time": now, "bucket": bucket, "data": decoded})
            if len(self.data_transfers) > 100:
                self.data_transfers.pop(0)

    def register_known_topics(self):
        """预注册已知的 OnDemand topic 映射 (不依赖 SEDP)"""
        known = [
            ("dsf/sys/var/tableDefine", "DSF::Var::PubTableDefine", "000003c2"),
            ("dsf/message/commandRequest/subTableRegister", "DSF::Message::SubTableRegister", "000004c2"),
        ]
        with self.lock:
            for tname, ttype, wid in known:
                if tname not in self.topic_writers:
                    self.topic_writers[tname] = {
                        "wid": wid,
                        "type": ttype,
                        "first_seen": time.time(),
                        "count": 0,
                    }
            # bucket topics: user writer entity_key = bucket + 3, kind = 0x03
            # writer ID 格式: entity_key(3字节) + kind(1字节) = "00000303"
            for i in range(20):
                tname = f"dsf/var/data/transfer/bucket_{i}"
                wid = f"{(i+3):06x}03"
                if tname not in self.topic_writers:
                    self.topic_writers[tname] = {
                        "wid": wid,
                        "type": "DSF::Var::TableDataTransfer",
                        "first_seen": time.time(),
                        "count": 0,
                    }

    def reset(self):
        """清空所有统计数据, 重新开始计数"""
        with self.lock:
            self.t0 = time.time()
            self.pkt_total = 0
            self.pkt_rtps = 0
            self.bytes_total = 0
            self.submsg_counts.clear()
            self.data_writers.clear()
            self.spdp_count = 0
            self.sedp_pub_count = 0
            self.sedp_sub_count = 0
            self.port_counts.clear()
            self.pkt_rate = RateMeter()
            self.byte_rate = RateMeter()
            self.data_rate = RateMeter()
            self.events.clear()
            # DDS 层不清除 topic_writers (只增不减，除非手动)
            self.topic_pkt_count.clear()
            self.topic_byte_count.clear()
            self.topic_rate.clear()
            self.pub_table_defines.clear()
            self.sub_registers.clear()
            self.data_transfers.clear()
            self.known_vars.clear()
            self.known_subs.clear()

    def snapshot(self):
        with self.lock:
            now = time.time()
            uptime = now - self.t0

            # 按 writer 分组: 区分 built-in (0xc2/0xc7) vs user (0x03)
            builtin_writers = {}
            user_writers = {}
            for wid, info in self.data_writers.items():
                entry = {
                    "count": info["count"],
                    "bytes": info["bytes"],
                    "rate": info["rate"].rate(),
                }
                kind = int(wid[-2:], 16)
                if kind == 0xc2:
                    builtin_writers[wid] = entry
                elif kind == 0x03:
                    # entity key = 前3字节, bucket = entity_key - 0x03
                    ekey = int(wid[:6], 16)
                    entry["bucket"] = ekey - 3
                    user_writers[wid] = entry

            # user writers 排序 (by entity key)
            sorted_user = sorted(user_writers.items(), key=lambda x: int(x[0][:6], 16))

            # DDS 层: topic 列表
            topic_list = []
            for tname, tinfo in sorted(self.topic_writers.items()):
                topic_list.append({
                    "name": tname,
                    "type": tinfo["type"],
                    "wid": tinfo["wid"],
                    "count": tinfo["count"],
                    "pkts": self.topic_pkt_count.get(tname, 0),
                    "bytes": self.topic_byte_count.get(tname, 0),
                    "rate": self.topic_rate[tname].rate() if tname in self.topic_rate else 0.0,
                })

            # 变量统计
            total_vars = len(self.known_vars)
            bucket_var_count = [0] * 20
            for v in self.known_vars.values():
                b = v.get("bucket", 0)
                if 0 <= b < 20:
                    bucket_var_count[b] += 1

            return {
                "uptime": uptime,
                "pkt_total": self.pkt_total,
                "pkt_rtps": self.pkt_rtps,
                "bytes_total": self.bytes_total,
                "pkt_rate": self.pkt_rate.rate(),
                "byte_rate": self.byte_rate.rate(),
                "data_rate": self.data_rate.rate(),
                "submsg_counts": dict(self.submsg_counts),
                "sorted_user": sorted_user,
                "builtin_writers": builtin_writers,
                "spdp_count": self.spdp_count,
                "sedp_pub_count": self.sedp_pub_count,
                "sedp_sub_count": self.sedp_sub_count,
                "port_counts": dict(self.port_counts),
                # DDS 层
                "topic_list": topic_list,
                "events": list(self.events),
                # DDS 层: 解码数据
                "pub_table_defines": list(self.pub_table_defines),
                "sub_registers": list(self.sub_registers),
                "data_transfers": list(self.data_transfers),
                "known_vars": dict(self.known_vars),
                "known_subs": dict(self.known_subs),
                "total_vars": total_vars,
                "bucket_var_count": bucket_var_count,
            }


# ═══════════════════════════════════════════════════════════════
#  抓包线程 (原生 socket)
# ═══════════════════════════════════════════════════════════════
class PcapWorker:
    def __init__(self, iface, stats, port_filter=None):
        self.iface = iface
        self.stats = stats
        self.port_filter = port_filter
        self.running = threading.Event()
        self.sock = None

    def start(self):
        self.running.set()
        threading.Thread(target=self._run, daemon=True, name="pcap").start()

    def stop(self):
        self.running.clear()
        if self.sock:
            try: self.sock.close()
            except: pass

    def _run(self):
        try:
            self.sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(0x0003))
            self.sock.bind((self.iface, 0))
            self.sock.settimeout(1.0)
            logging.info("pcap started iface=%s filter=%s", self.iface, self.port_filter)
            print(f"[INFO] 开始抓包: iface={self.iface}, port_filter={self.port_filter}", file=sys.stderr)
        except PermissionError as e:
            logging.error("socket permission error: %s", e)
            print(f"[ERROR] 需要root权限: {e}", file=sys.stderr)
            return
        except OSError as e:
            logging.error("socket bind error: %s", e)
            print(f"[ERROR] 网卡{self.iface}不存在或无法绑定: {e}", file=sys.stderr)
            return

        pkt_count = 0
        while self.running.is_set():
            try:
                data, _ = self.sock.recvfrom(65535)
                self._process(data)
                pkt_count += 1
                if pkt_count % 1000 == 0:
                    logging.debug("processed %d packets", pkt_count)
            except socket.timeout:
                continue
            except Exception:
                pass

    def _process(self, data):
        if len(data) < 42:  # ETH(14) + IP(20) + UDP(8)
            return

        # Parse Ethernet -> IPv4 -> UDP
        etype = struct.unpack("!H", data[12:14])[0]
        if etype != 0x0800:
            return
        ihl = (data[14] & 0x0F) * 4
        if data[23] != 17:  # UDP
            return
        udp_start = 14 + ihl
        if len(data) < udp_start + 8:
            return

        sport, dport = struct.unpack("!HH", data[udp_start:udp_start+4])
        udp_len = struct.unpack("!H", data[udp_start+4:udp_start+6])[0]
        payload = data[udp_start+8:udp_start+udp_len]

        # 端口过滤
        if self.port_filter:
            if sport not in self.port_filter and dport not in self.port_filter:
                return

        pkt_len = len(payload)
        self.stats.on_packet(pkt_len, sport, dport)

        # RTPS check
        if pkt_len < 20 or payload[:4] != RTPS_MAGIC:
            return
        self.stats.on_rtps()

        # Parse submessages
        offset = 20
        while offset + 4 <= len(payload):
            sid = payload[offset]
            flags = payload[offset + 1]
            is_be = not (flags & 0x01)
            fmt = "!H" if is_be else "<H"
            length = struct.unpack_from(fmt, payload, offset + 2)[0]
            if length == 0:
                break

            if sid in (0x15, 0x16) and length >= 16:  # DATA / DATA_FRAG
                body = offset + 4
                wid = payload[body+8:body+12].hex()
                port = dport if dport in (7400, 7401) else sport
                self.stats.on_submsg(sid, wid_hex=wid, frag_len=length, port=port)

                # SEDP discovery: 解析 inline QoS 提取 topic name
                if wid in ("000003c2", "000004c2"):  # SEDP-Pub / SEDP-Sub
                    self._parse_sedp(payload, offset, length, is_be, wid)

                # 解码 DATA payload (只处理非分片消息)
                if sid == 0x15:
                    self._decode_data_payload(payload, offset, length, is_be, wid)
            else:
                self.stats.on_submsg(sid)

            offset += 4 + length

    def _parse_sedp(self, payload, submsg_offset, submsg_len, is_be, wid_hex):
        """解析 SEDP DATA 子消息，提取 inline QoS 中的 topic name"""
        try:
            body = submsg_offset + 4
            octets_to_inline = struct.unpack_from(">H" if is_be else "<H", payload, body + 2)[0]
            inline_start = body + octets_to_inline
            inline_end = submsg_offset + 4 + submsg_len

            if inline_start >= inline_end or inline_start >= len(payload):
                return

            topic_name = extract_inline_qos_topic(payload, inline_start, inline_end, is_be)
            type_name = extract_inline_qos_type(payload, inline_start, inline_end, is_be)

            if topic_name:
                self.stats.on_topic_discover(topic_name, type_name, wid_hex)
                logging.info("SEDP discovered: topic=%s type=%s wid=%s", topic_name, type_name, wid_hex)
        except Exception as e:
            logging.debug("SEDP parse error: %s", e)

    def _find_payload_offset(self, payload, inline_start, inline_end, is_be):
        """找到 inline QoS 之后的序列化 payload 起始位置"""
        fmt = ">H" if is_be else "<H"
        offset = inline_start
        while offset + 4 <= inline_end:
            pid = struct.unpack_from(fmt, payload, offset)[0]
            plen = struct.unpack_from(fmt, payload, offset + 2)[0]
            if pid == 0x0001:  # PID_SENTINEL
                offset += 4
                # 可能有 padding 对齐到4
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

    def _decode_data_payload(self, payload, submsg_offset, submsg_len, is_be, wid_hex):
        """解码 DATA 子消息的序列化 payload"""
        try:
            body = submsg_offset + 4
            octets_to_inline = struct.unpack_from(">H" if is_be else "<H", payload, body + 2)[0]
            inline_start = body + octets_to_inline
            inline_end = submsg_offset + 4 + submsg_len

            if inline_start >= inline_end or inline_start >= len(payload):
                return

            # 找到 payload 起始
            payload_start = self._find_payload_offset(payload, inline_start, inline_end, is_be)
            if payload_start is None or payload_start >= inline_end:
                return

            # 根据 writer ID 判断 topic 类型
            # SEDP-Pub (0x000003c2) -> PubTableDefine
            if wid_hex == "000003c2":
                decoded = decode_pub_table_define(payload, payload_start, is_be)
                if decoded:
                    self.stats.on_pub_table_define(decoded)
                    logging.info("Decoded PubTableDefine: %s/%s, %d vars",
                                 decoded.get("nodeName"), decoded.get("name"), decoded.get("var_count", 0))

            # SEDP-Sub (0x000004c2) -> SubTableRegister
            elif wid_hex == "000004c2":
                decoded = decode_sub_table_register(payload, payload_start, is_be)
                if decoded:
                    self.stats.on_sub_register(decoded)
                    logging.info("Decoded SubTableRegister: %s, %d vars",
                                 decoded.get("nodeName"), decoded.get("var_count", 0))

            # User writers (0x03) -> TableDataTransfer
            elif wid_hex.endswith("03"):
                decoded = decode_table_data_transfer(payload, payload_start, is_be)
                if decoded:
                    ekey = int(wid_hex[:6], 16)
                    bucket = ekey - 3
                    self.stats.on_data_transfer(bucket, decoded)

        except Exception as e:
            logging.warning("Decode payload error: wid=%s offset=%d len=%d err=%s",
                          wid_hex, submsg_offset, submsg_len, e)


# ═══════════════════════════════════════════════════════════════
#  TUI
# ═══════════════════════════════════════════════════════════════
C_GRN = 1; C_YEL = 2; C_RED = 3; C_CYA = 4; C_HDR = 5; C_DIM = 6; C_WHT = 7

def init_colors():
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(C_GRN, curses.COLOR_GREEN, -1)
    curses.init_pair(C_YEL, curses.COLOR_YELLOW, -1)
    curses.init_pair(C_RED, curses.COLOR_RED, -1)
    curses.init_pair(C_CYA, curses.COLOR_CYAN, -1)
    curses.init_pair(C_HDR, curses.COLOR_WHITE, curses.COLOR_BLUE)
    curses.init_pair(C_DIM, curses.COLOR_WHITE, -1)
    curses.init_pair(C_WHT, curses.COLOR_WHITE, -1)

def safe(s, y, x, text, attr=0, mx=None):
    my, m = s.getmaxyx()
    if mx: m = min(m, mx)
    if y < 0 or y >= my or x < 0: return
    avail = m - x
    if avail <= 0: return
    try: s.addnstr(y, x, text[:avail], avail, attr)
    except: pass

def fmt_b(n):
    for u in ["B", "KB", "MB", "GB"]:
        if n < 1024: return f"{n:.1f}{u}"
        n /= 1024
    return f"{n:.1f}TB"

def fmt_r(n):
    if n >= 1e6: return f"{n/1e6:.1f}M"
    if n >= 1e3: return f"{n/1e3:.1f}K"
    return f"{n:.0f}"

def fmt_freq(rate):
    """将速率(每秒)转换为频率显示 (如10ms, 100ms, 1s)"""
    if rate <= 0:
        return "--"
    interval = 1.0 / rate  # 秒
    if interval < 0.001:
        return f"{interval*1e6:.0f}us"
    elif interval < 1.0:
        return f"{interval*1e3:.0f}ms"
    else:
        return f"{interval:.1f}s"

def hcolor(v, w, c):
    return curses.color_pair(C_RED if v >= c else C_YEL if v >= w else C_GRN)

def hlabel(v, w, c):
    return "CRIT" if v >= c else "WARN" if v >= w else "OK"

def sep(s, y, mx, label=""):
    if label:
        safe(s, y, 2, f"── {label} ", curses.color_pair(C_DIM), mx)
    else:
        safe(s, y, 2, "─" * (mx - 4), curses.color_pair(C_DIM))

VIEWS = ["Overview", "Writers", "Protocol", "Topics", "Variables"]


def draw_overview(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll
    up = time.strftime("%H:%M:%S", time.gmtime(snap["uptime"]))
    if y >= 3: safe(s, y, 2, f" Uptime: {up}    Packets: {snap['pkt_total']:>10,}    RTPS: {snap['pkt_rtps']:>10,}    Bytes: {fmt_b(snap['bytes_total']):>10s}", mx=mx)
    y += 1
    if y >= 3: safe(s, y, 2, f" Pkt Rate: {fmt_r(snap['pkt_rate'])}/s    Byte Rate: {fmt_b(snap['byte_rate'])}/s    DATA Rate: {fmt_r(snap['data_rate'])}/s", mx=mx)
    y += 2

    # Port stats
    if y >= 3: sep(s, y, mx, "Port Distribution")
    y += 1
    for port in sorted(snap.get("port_counts", {}).keys()):
        if y >= my - 2: break
        cnt = snap["port_counts"][port]
        label = "discovery" if port == 7400 else "userdata"
        if y >= 3: safe(s, y, 4, f"  Port {port} ({label}): {cnt:>10,} packets", mx=mx)
        y += 1
    y += 1

    # Submessage types
    if y >= 3: sep(s, y, mx, "RTPS Submessage Types")
    y += 1
    for sid in sorted(snap["submsg_counts"].keys(), key=lambda x: snap["submsg_counts"][x], reverse=True):
        if y >= my - 2: break
        cnt = snap["submsg_counts"][sid]
        name = SUBMSG_NAMES.get(sid, f"0x{sid:02x}")
        if y >= 3: safe(s, y, 4, f"  {name:<12s}: {cnt:>10,}", mx=mx)
        y += 1
    y += 1

    # Discovery stats
    if y >= 3: sep(s, y, mx, "Discovery (Built-in)")
    y += 1
    if y >= 3: safe(s, y, 4, f"  SPDP Participant: {snap['spdp_count']:>8,}", curses.color_pair(C_CYA), mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  SEDP Publications: {snap['sedp_pub_count']:>7,}", curses.color_pair(C_CYA), mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  SEDP Subscriptions: {snap['sedp_sub_count']:>6,}", curses.color_pair(C_CYA), mx=mx)


def draw_writers(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll

    # User writers (data plane)
    if y >= 3: sep(s, y, mx, "User Writers (Data Plane)")
    y += 1
    if y >= 3:
        safe(s, y, 4, f"{'Bucket':>6s} {'WriterId':<10s} {'EntityKey':<10s} {'Pkts':>8s} {'Bytes':>10s} {'Rate':>8s}", curses.A_UNDERLINE, mx=mx)
    y += 1

    writers = snap.get("sorted_user", [])
    for wid, info in writers:
        if y >= my - 2: break
        if y < 3: y += 1; continue
        ekey = wid[:6]
        bucket = info.get("bucket", -1)
        safe(s, y, 4, f"bucket_{bucket:<2d} {wid:<10s} 0x{ekey:<8s} {info['count']:>8,} {fmt_b(info['bytes']):>10s} {fmt_r(info['rate'])+'/s':>8s}", mx=mx)
        y += 1

    y += 1
    # Summary
    total_data = sum(info["count"] for _, info in writers)
    total_bytes = sum(info["bytes"] for _, info in writers)
    total_rate = sum(info["rate"] for _, info in writers)
    if y >= 3: safe(s, y, 4, f"  Total: {len(writers)} writers, {total_data:,} packets, {fmt_b(total_bytes)}, {fmt_r(total_rate)}/s",
                     curses.color_pair(C_GRN), mx=mx)
    y += 2

    # Built-in writers
    if y >= 3: sep(s, y, mx, "Built-in Writers (Discovery)")
    y += 1
    for wid, info in snap.get("builtin_writers", {}).items():
        if y >= my - 2: break
        if y < 3: y += 1; continue
        label = "SPDP" if wid == "000100c2" else ("SEDP-Pub" if wid == "000003c2" else ("SEDP-Sub" if wid == "000004c2" else "Unknown"))
        safe(s, y, 4, f"  {wid} ({label}): {info['count']:>8,} pkts  {fmt_r(info['rate'])}/s",
             curses.color_pair(C_CYA), mx=mx)
        y += 1


def draw_protocol(s, snap, cfg, scroll, my, mx):
    y = 3 - scroll

    # Heartbeat / ACK / GAP analysis
    hb = snap["submsg_counts"].get(0x07, 0)
    ack = snap["submsg_counts"].get(0x06, 0)
    gap = snap["submsg_counts"].get(0x08, 0)
    data = snap["submsg_counts"].get(0x15, 0)
    frag = snap["submsg_counts"].get(0x16, 0)
    total_data = data + frag

    if y >= 3: sep(s, y, mx, "Protocol Health")
    y += 1

    # HB loss rate
    hb_loss = max(0, 1.0 - ack / hb) if hb > 0 else 0
    hb_pct = hb_loss * 100
    if y >= 3:
        safe(s, y, 4, f"  HB Loss Rate:    {hb_pct:>10.4f}%  ", mx=mx)
        safe(s, y, 38, f"[{hlabel(hb_loss, 0.01, 0.05)}]", hcolor(hb_loss, 0.01, 0.05), mx=mx)
    y += 1

    # Resend rate
    resend = gap / total_data if total_data > 0 else 0
    resend_pct = resend * 100
    if y >= 3:
        safe(s, y, 4, f"  Resend Rate:     {resend_pct:>10.4f}%  ", mx=mx)
        safe(s, y, 38, f"[{hlabel(resend, 0.005, 0.02)}]", hcolor(resend, 0.005, 0.02), mx=mx)
    y += 2

    # Message rates
    if y >= 3: sep(s, y, mx, "Message Rates (avg)")
    y += 1
    up = max(1, snap["uptime"])
    rates = [
        ("HEARTBEAT", hb / up, C_CYA),
        ("ACKNACK", ack / up, C_YEL),
        ("DATA", data / up, C_GRN),
        ("DATA_FRAG", frag / up, C_GRN),
        ("GAP", gap / up, C_RED),
    ]
    for name, rate, color in rates:
        if y >= my - 2: break
        if y >= 3: safe(s, y, 4, f"  {name:<12s}: {fmt_r(rate)+'/s':>10s}", curses.color_pair(color), mx=mx)
        y += 1

    y += 1
    if y >= 3: sep(s, y, mx, "Throughput")
    y += 1
    if y >= 3: safe(s, y, 4, f"  Current:  {fmt_b(snap['byte_rate'])}/s", mx=mx)
    y += 1
    if y >= 3: safe(s, y, 4, f"  Total:    {fmt_b(snap['bytes_total'])}", mx=mx)
    y += 1
    avg = snap["bytes_total"] / up
    if y >= 3: safe(s, y, 4, f"  Average:  {fmt_b(avg)}/s", mx=mx)


def draw_topics(s, snap, cfg, scroll, my, mx):
    """F4: DDS Topics 视图 - 显示 SEDP 发现的 topic 和统计"""
    y = 3 - scroll

    topics = snap.get("topic_list", [])

    # Topic 列表
    if y >= 3: sep(s, y, mx, "DDS Topics (SEDP Discovery)")
    y += 1
    if y >= 3:
        safe(s, y, 4, f"{'Topic':<45s} {'WriterId':<10s} {'Pkts':>8s} {'Bytes':>10s} {'Freq':>8s}", curses.A_UNDERLINE, mx=mx)
    y += 1

    if not topics:
        if y >= 3: safe(s, y, 4, "  (等待 SEDP 发现包... 需要 pub/sub 启动)", curses.color_pair(C_YEL), mx=mx)
        y += 1
    else:
        # 区分 OnDemand topic 和其他 topic
        ondemand_topics = [t for t in topics if t["name"].startswith("dsf/")]
        other_topics = [t for t in topics if not t["name"].startswith("dsf/")]

        for t in ondemand_topics:
            if y >= my - 2: break
            if y < 3: y += 1; continue
            # 高亮已知 topic
            is_known = t["name"] in KNOWN_TOPICS
            attr = curses.color_pair(C_GRN) if is_known else 0
            safe(s, y, 4, f"{t['name']:<45.45s} {t['wid']:<10s} {t['pkts']:>8,} {fmt_b(t['bytes']):>10s} {fmt_freq(t['rate']):>8s}", attr, mx=mx)
            y += 1

        if other_topics and y < my - 2:
            y += 1
            if y >= 3: sep(s, y, mx, "Other Topics")
            y += 1
            for t in other_topics:
                if y >= my - 2: break
                if y < 3: y += 1; continue
                safe(s, y, 4, f"{t['name']:<45.45s} {t['wid']:<10s} {t['pkts']:>8,} {fmt_b(t['bytes']):>10s} {fmt_freq(t['rate']):>8s}", curses.color_pair(C_DIM), mx=mx)
                y += 1

    y += 2

    # Summary
    total_topics = len(topics)
    ondemand_count = len([t for t in topics if t["name"].startswith("dsf/")])
    if y >= 3:
        safe(s, y, 4, f"  发现 {total_topics} 个 topic, 其中 {ondemand_count} 个 OnDemand topic",
             curses.color_pair(C_CYA), mx=mx)
    y += 2

    # 最近事件
    events = snap.get("events", [])
    if events and y < my - 2:
        if y >= 3: sep(s, y, mx, "最近 SEDP 事件")
        y += 1
        for ev in events[-10:]:
            if y >= my - 2: break
            if y < 3: y += 1; continue
            safe(s, y, 4, f"  {ev}", curses.color_pair(C_DIM), mx=mx)
            y += 1


def draw_variables(s, snap, cfg, scroll, my, mx):
    """F5: Variables 视图 - 显示解码后的变量定义、订阅和传输"""
    y = 3 - scroll

    known_vars = snap.get("known_vars", {})
    known_subs = snap.get("known_subs", {})
    pub_defines = snap.get("pub_table_defines", [])
    sub_registers = snap.get("sub_registers", [])
    bucket_var_count = snap.get("bucket_var_count", [0] * 20)
    total_vars = snap.get("total_vars", 0)

    # 无数据提示
    if total_vars == 0 and not pub_defines and not known_subs:
        if y >= 3: sep(s, y, mx, "Variables")
        y += 1
        if y >= 3:
            safe(s, y, 4, "  (等待 PubTableDefine / SubTableRegister 消息...)", curses.color_pair(C_YEL), mx=mx)
        y += 1
        if y >= 3:
            safe(s, y, 4, "  需要 sudo 权限抓包，且 pub/sub 正在运行", curses.color_pair(C_DIM), mx=mx)
        y += 1
        if y >= 3:
            safe(s, y, 4, f"  当前 topic 数: {len(snap.get('topic_list', []))}", curses.color_pair(C_DIM), mx=mx)
        return

    # 总览
    if y >= 3: sep(s, y, mx, "变量总览")
    y += 1
    if y >= 3:
        safe(s, y, 4, f"  已知变量: {total_vars}    Pub节点: {len(pub_defines)}    Sub节点: {len(known_subs)}",
             curses.color_pair(C_CYA), mx=mx)
    y += 2

    # Bucket 分布
    if y >= 3: sep(s, y, mx, "Bucket 变量分布")
    y += 1
    max_var = max(bucket_var_count) if bucket_var_count else 1
    bar_width = max(1, mx - 30)
    for i in range(20):
        if y >= my - 2: break
        if y < 3: y += 1; continue
        cnt = bucket_var_count[i]
        bar_len = int(cnt * bar_width / max(max_var, 1))
        safe(s, y, 4, f"  bucket_{i:<2d}: {cnt:>5d} ", mx=mx)
        if cnt > 0:
            safe(s, y, 20, "█" * min(bar_len, bar_width), curses.color_pair(C_GRN), mx=mx)
        y += 1
    y += 1

    # Sub 订阅信息
    if known_subs and y < my - 2:
        if y >= 3: sep(s, y, mx, "订阅请求 (SubTableRegister)")
        y += 1
        for node, info in sorted(known_subs.items()):
            if y >= my - 2: break
            if y < 3: y += 1; continue
            table = info.get("tableName", "")
            msg_type = "SUB" if info.get("msgType", 0) == 0 else "UNSUB"
            var_count = len(info.get("vars", []))
            safe(s, y, 4, f"  {node:<20.20s} -> {table:<15.15s} [{msg_type}] {var_count} vars",
                 curses.color_pair(C_YEL), mx=mx)
            y += 1
            # 显示前几个变量的频率
            for v in info.get("vars", [])[:5]:
                if y >= my - 2: break
                safe(s, y, 8, f"    {v['name']:<30.30s} freq={v['freq']}", curses.color_pair(C_DIM), mx=mx)
                y += 1
            if len(info.get("vars", [])) > 5:
                safe(s, y, 8, f"    ... and {len(info['vars'])-5} more", curses.color_pair(C_DIM), mx=mx)
                y += 1
        y += 1

    # 最近的 PubTableDefine 消息
    if pub_defines and y < my - 2:
        if y >= 3: sep(s, y, mx, "变量定义 (PubTableDefine)")
        y += 1
        # 只显示最新的几条
        for entry in pub_defines[-5:]:
            if y >= my - 2: break
            if y < 3: y += 1; continue
            data = entry.get("data", {})
            ts = time.strftime("%H:%M:%S", time.localtime(entry.get("time", 0)))
            node = data.get("nodeName", "")
            table = data.get("name", "")
            var_count = data.get("var_count", 0)
            safe(s, y, 4, f"  [{ts}] {node}/{table}: {var_count} vars", curses.color_pair(C_GRN), mx=mx)
            y += 1
            # 显示前几个变量
            for v in data.get("vars", [])[:3]:
                if y >= my - 2: break
                vid = v.get("id", 0)
                vname = v.get("name", "")
                vsize = v.get("size", 0)
                safe(s, y, 8, f"    0x{vid:016x} {vname:<25.25s} size={vsize}", curses.color_pair(C_DIM), mx=mx)
                y += 1
            if len(data.get("vars", [])) > 3:
                safe(s, y, 8, f"    ... and {len(data['vars'])-3} more", curses.color_pair(C_DIM), mx=mx)
                y += 1
        y += 1

    # 变量列表 (按 hash 排序)
    if known_vars and y < my - 2:
        if y >= 3: sep(s, y, mx, "变量列表 (top 30)")
        y += 1
        if y >= 3:
            safe(s, y, 4, f"  {'VarHash':<18s} {'Name':<25s} {'Node':<15s} {'Bucket':>6s} {'Size':>6s}",
                 curses.A_UNDERLINE, mx=mx)
        y += 1
        sorted_vars = sorted(known_vars.items(), key=lambda x: x[0])[:30]
        for vid, v in sorted_vars:
            if y >= my - 2: break
            if y < 3: y += 1; continue
            safe(s, y, 4, f"  0x{vid:016x} {v['name']:<25.25s} {v['nodeName']:<15.15s} {v['bucket']:>6d} {v['size']:>6d}",
                 mx=mx)
            y += 1


def tui(stdscr, cfg, stats):
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.timeout(1000 // cfg["hz"])
    init_colors()

    view = 1
    scroll = 0
    paused = False
    num_views = len(VIEWS)

    while True:
        k = stdscr.getch()
        if k in (ord("q"), ord("Q")): break
        elif k in (ord("p"), ord("P")): paused = not paused
        elif k in (ord("c"), ord("C")):
            stats.reset()
            scroll = 0
        elif k == curses.KEY_LEFT:
            view = max(1, view - 1); scroll = 0
        elif k == curses.KEY_RIGHT:
            view = min(num_views, view + 1); scroll = 0
        elif k == curses.KEY_UP: scroll = max(0, scroll - 1)
        elif k == curses.KEY_DOWN: scroll += 1

        if paused:
            time.sleep(0.1)
            continue

        snap = stats.snapshot()
        stdscr.erase()
        my, mx = stdscr.getmaxyx()

        # Header
        title = " OnDemand DDS Monitor "
        safe(stdscr, 0, max(0, (mx - len(title)) // 2), title, curses.color_pair(C_HDR) | curses.A_BOLD, mx)
        x = 2
        for i, name in enumerate(VIEWS):
            vid = i + 1
            attr = (curses.color_pair(C_HDR) | curses.A_BOLD) if vid == view else curses.color_pair(C_CYA)
            label = f" [{vid}] {name} "
            safe(stdscr, 1, x, label, attr, mx)
            x += len(label) + 1

        # Footer
        pause_str = " [PAUSED]" if paused else ""
        footer = f" iface={cfg['iface']}  domain={cfg['domain']}{pause_str}  [←→]Switch [↑↓]Scroll [P]Pause [C]Clear [Q]Quit"
        safe(stdscr, my - 1, 0, footer[:mx], curses.color_pair(C_DIM), mx)

        if view == 1: draw_overview(stdscr, snap, cfg, scroll, my, mx)
        elif view == 2: draw_writers(stdscr, snap, cfg, scroll, my, mx)
        elif view == 3: draw_protocol(stdscr, snap, cfg, scroll, my, mx)
        elif view == 4: draw_topics(stdscr, snap, cfg, scroll, my, mx)
        elif view == 5: draw_variables(stdscr, snap, cfg, scroll, my, mx)

        stdscr.refresh()


# ═══════════════════════════════════════════════════════════════
#  入口
# ═══════════════════════════════════════════════════════════════
def print_stats(stats, duration=5, interval=1):
    """非交互模式: 终端打印统计"""
    print(f"Monitoring for {duration}s (Ctrl+C to stop)...")
    try:
        for i in range(duration):
            time.sleep(interval)
            snap = stats.snapshot()
            up = snap["uptime"]
            print(f"\n--- t={up:.1f}s ---")
            print(f"  Packets: {snap['pkt_total']:>8,}  RTPS: {snap['pkt_rtps']:>8,}  Bytes: {fmt_b(snap['bytes_total'])}")
            print(f"  Rates:   {fmt_r(snap['pkt_rate'])}/s pkt  {fmt_b(snap['byte_rate'])}/s byte  {fmt_r(snap['data_rate'])}/s data")
            print(f"  Ports:   {snap['port_counts']}")
            names = {0x06:"ACKNACK",0x07:"HEARTBEAT",0x08:"GAP",0x09:"INFO_TS",0x0e:"INFO_DST",0x15:"DATA",0x16:"DATA_FRAG",0x80:"VENDOR"}
            sub = ", ".join(f"{names.get(k,f'0x{k:02x}')}={v}" for k,v in sorted(snap["submsg_counts"].items(), key=lambda x:-x[1]))
            print(f"  Submsgs: {sub}")
            print(f"  Writers: {len(snap['sorted_user'])} user, {len(snap['builtin_writers'])} built-in")
            for wid, info in snap["sorted_user"][:5]:
                bucket = info.get("bucket", -1)
                print(f"    bucket_{bucket} ({wid}): {info['count']} pkts, {fmt_r(info['rate'])}/s")
            if len(snap["sorted_user"]) > 5:
                print(f"    ... and {len(snap['sorted_user'])-5} more")
            # DDS Topics
            topics = snap.get("topic_list", [])
            if topics:
                print(f"  Topics: {len(topics)} discovered")
                for t in topics[:5]:
                    print(f"    {t['name']} ({t['wid']}): {t['pkts']} pkts, freq={fmt_freq(t['rate'])}")
                if len(topics) > 5:
                    print(f"    ... and {len(topics)-5} more")
            # DDS Variables
            total_vars = snap.get("total_vars", 0)
            known_subs = snap.get("known_subs", {})
            pub_defines = snap.get("pub_table_defines", [])
            if total_vars > 0 or known_subs or pub_defines:
                print(f"  Variables: {total_vars} known")
                print(f"  PubTableDefine: {len(pub_defines)} messages")
                for entry in pub_defines[-3:]:
                    data = entry.get("data", {})
                    print(f"    {data.get('nodeName')}/{data.get('name')}: {data.get('var_count', 0)} vars")
                print(f"  SubTableRegister: {len(known_subs)} nodes")
                for node, info in list(known_subs.items())[:3]:
                    print(f"    {node} -> {info.get('tableName')}: {len(info.get('vars', []))} vars")
    except KeyboardInterrupt:
        pass
    snap = stats.snapshot()
    print(f"\n=== Final ({snap['uptime']:.1f}s) ===")
    print(f"  Total: {snap['pkt_total']:,} pkts  {snap['pkt_rtps']:,} RTPS  {fmt_b(snap['bytes_total'])}")
    print(f"  Writers: {len(snap['sorted_user'])} user, {len(snap['builtin_writers'])} built-in")
    for wid, info in snap["sorted_user"]:
        bucket = info.get("bucket", -1)
        print(f"    bucket_{bucket} ({wid}): {info['count']} pkts, {fmt_b(info['bytes'])}, {fmt_r(info['rate'])}/s")
    # Final variables
    total_vars = snap.get("total_vars", 0)
    if total_vars > 0:
        print(f"  Variables: {total_vars} known")
        bucket_var = snap.get("bucket_var_count", [])
        for i, cnt in enumerate(bucket_var):
            if cnt > 0:
                print(f"    bucket_{i}: {cnt} vars")


def main():
    p = argparse.ArgumentParser(description="OnDemand DDS Monitor")
    p.add_argument("-i", "--interface", default="lo", help="网卡名")
    p.add_argument("-d", "--domain", type=int, default=66, help="DDS domain")
    p.add_argument("--hz", type=int, default=2, help="刷新频率")
    p.add_argument("--debug", action="store_true", help="抓所有UDP")
    p.add_argument("--print", action="store_true", help="非交互模式(终端打印)")
    p.add_argument("--duration", type=int, default=10, help="非交互模式持续时间(s)")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    # 检查 root 权限
    if os.geteuid() != 0:
        print("错误: 抓包需要 root 权限，请使用 sudo 运行", file=sys.stderr)
        print(f"  sudo python3 {sys.argv[0]} -i {args.interface}", file=sys.stderr)
        sys.exit(1)

    level = logging.DEBUG if args.verbose else logging.WARNING
    logging.basicConfig(level=level, filename="monitor.log", filemode="w",
                        format="[%(asctime)s] %(message)s")

    ports = None if args.debug else {7400, 7401}
    cfg = {"iface": args.interface, "domain": args.domain, "hz": args.hz}

    print(f"[INFO] 启动 DDS Monitor", file=sys.stderr)
    print(f"[INFO] 网卡: {args.interface}", file=sys.stderr)
    print(f"[INFO] 端口过滤: {'全部' if args.debug else '7400,7401'}", file=sys.stderr)
    print(f"[INFO] 日志级别: {'DEBUG' if args.verbose else 'WARNING'}", file=sys.stderr)

    stats = Stats()
    stats.register_known_topics()  # 预注册已知 topic
    pcap = PcapWorker(args.interface, stats, port_filter=ports)
    pcap.start()
    time.sleep(0.5)  # 等待抓包线程启动

    logging.info("started iface=%s domain=%d ports=%s", args.interface, args.domain, ports)

    # 检查是否有流量
    if args.print:
        time.sleep(1)
        total_pkts = stats.pkt_total
        if total_pkts == 0:
            print(f"[WARNING] 1秒内未收到任何UDP包，请检查:", file=sys.stderr)
            print(f"  1. 是否有DDS应用在运行?", file=sys.stderr)
            print(f"  2. 网卡{args.interface}是否正确?", file=sys.stderr)
            print(f"  3. 是否使用了sudo?", file=sys.stderr)

    try:
        if args.print:
            print_stats(stats, duration=args.duration)
        else:
            curses.wrapper(lambda s: tui(s, cfg, stats))
    except KeyboardInterrupt:
        pass
    finally:
        pcap.stop()

if __name__ == "__main__":
    main()
