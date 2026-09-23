"""Immutable Stage2 packed-image reader (stdlib only)."""
from dataclasses import dataclass
import binascii
import struct

SLOT_SIZE = 0x38000
HEADER_OFFSET = 0x37000
SLOT_BASES = (0x8000, 0x40000)

def version_text(value: int) -> str:
    return ".".join(str((value >> shift) & 255) for shift in (24, 16, 8, 0))

@dataclass(frozen=True)
class PackedImage:
    data: bytes
    slot: int
    version: int
    image_size: int
    image_crc: int
    build_id: int

    @property
    def address(self) -> int:
        return SLOT_BASES[self.slot]

    @classmethod
    def parse(cls, data: bytes) -> "PackedImage":
        if len(data) != SLOT_SIZE:
            raise ValueError("Expected full Stage2 packed slot image (229376 bytes), not raw APP.bin")
        header = data[HEADER_OFFSET:HEADER_OFFSET + 64]
        fields = struct.unpack("<16I", header)
        magic, fmt, size, crc, version, build_id, vector, entry = fields[:8]
        if magic != 0x42493256 or fmt != 1:
            raise ValueError("Bad image magic/header version")
        if binascii.crc32(header[:60]) & 0xFFFFFFFF != fields[15]:
            raise ValueError("Bad Header CRC")
        if not 8 <= size <= HEADER_OFFSET:
            raise ValueError("Payload size outside slot")
        if vector not in SLOT_BASES:
            raise ValueError("Bad vector address")
        msp, reset = struct.unpack_from("<II", data)
        if not 0x1FFF8000 <= msp <= 0x20006FF0 or msp % 8:
            raise ValueError("Bad initial MSP")
        if not reset & 1 or not vector <= (reset & ~1) < vector + size or entry != reset:
            raise ValueError("Bad Reset Handler/entry")
        if binascii.crc32(data[:size]) & 0xFFFFFFFF != crc:
            raise ValueError("Bad Payload CRC")
        if any(value != 255 for value in data[HEADER_OFFSET + 64:]):
            raise ValueError("Non-erased reserved header tail")
        return cls(data, SLOT_BASES.index(vector), version, size, crc, build_id)

def transfer_chunks(data: bytes, max_block_length: int):
    """maxNumberOfBlockLength includes SID and BSC; no fixed 128-byte cap."""
    if not 3 <= max_block_length <= 4095:
        raise ValueError("ECU advertised unusable maxNumberOfBlockLength")
    length = max_block_length - 2
    for offset in range(0, len(data), length):
        yield data[offset:offset + length]
