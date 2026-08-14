"""OnDemand DDS Monitor v2.0 - CDR (Common Data Representation) decoder"""
import struct
from typing import Tuple


class CDRReader:
    """CDR decoder supporting CDR_LE / CDR_BE"""

    def __init__(self, buf: bytes, offset: int = 0, is_be: bool = False):
        self.buf = buf
        self.off = offset
        self.is_be = is_be

    def _fmt(self, code: str) -> str:
        return (">" if self.is_be else "<") + code

    def align(self, n: int):
        r = self.off % n
        if r:
            self.off += n - r

    def read_i32(self) -> int:
        self.align(4)
        v = struct.unpack_from(self._fmt("i"), self.buf, self.off)[0]
        self.off += 4
        return v

    def read_u32(self) -> int:
        self.align(4)
        v = struct.unpack_from(self._fmt("I"), self.buf, self.off)[0]
        self.off += 4
        return v

    def read_i64(self) -> int:
        self.align(8)
        v = struct.unpack_from(self._fmt("q"), self.buf, self.off)[0]
        self.off += 8
        return v

    def read_u64(self) -> int:
        self.align(8)
        v = struct.unpack_from(self._fmt("Q"), self.buf, self.off)[0]
        self.off += 8
        return v

    def read_bool(self) -> bool:
        v = struct.unpack_from("B", self.buf, self.off)[0]
        self.off += 1
        return v != 0

    def read_u8(self) -> int:
        v = struct.unpack_from("B", self.buf, self.off)[0]
        self.off += 1
        return v

    def read_string(self) -> str:
        slen = self.read_u32()
        if slen == 0:
            return ""
        s = self.buf[self.off:self.off + slen - 1].decode("utf-8", errors="replace")
        self.off += slen
        self.align(4)
        return s

    def read_bytes(self, n: int) -> bytes:
        v = self.buf[self.off:self.off + n]
        self.off += n
        return v

    def remaining(self) -> int:
        return len(self.buf) - self.off

    def skip(self, n: int):
        self.off += n


def cdr_read_string(buf: bytes, offset: int, is_be: bool) -> Tuple[str, int]:
    """Read a CDR string from buffer at offset, return (string, new_offset)"""
    if offset + 4 > len(buf):
        return "", offset
    fmt = ">I" if is_be else "<I"
    slen = struct.unpack_from(fmt, buf, offset)[0]
    offset += 4
    if slen == 0 or offset + slen > len(buf):
        return "", offset
    s = buf[offset:offset + slen - 1].decode("utf-8", errors="replace")
    offset += slen
    if offset % 4:
        offset += 4 - (offset % 4)
    return s, offset