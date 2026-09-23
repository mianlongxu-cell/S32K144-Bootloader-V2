"""Small standard-library SocketCAN wrapper for Classic CAN frames."""

from __future__ import annotations

from dataclasses import dataclass
import socket
import struct
from typing import Optional


CAN_FRAME_FORMAT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FORMAT)
CAN_SFF_MASK = 0x7FF
CAN_EFF_FLAG = 0x80000000
CAN_RTR_FLAG = 0x40000000
CAN_ERR_FLAG = 0x20000000


@dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: bytes

    def __post_init__(self) -> None:
        if not 0 <= self.can_id <= CAN_SFF_MASK:
            raise ValueError("Only 11-bit standard CAN identifiers are supported")
        if len(self.data) > 8:
            raise ValueError("Classic CAN payload cannot exceed 8 bytes")


class SocketCanBus:
    """Blocking SocketCAN transport with optional receive timeout."""

    def __init__(self, channel: str) -> None:
        self.channel = channel
        self._socket: Optional[socket.socket] = None

    def open(self) -> None:
        if self._socket is not None:
            return
        if not hasattr(socket, "AF_CAN"):
            raise OSError("SocketCAN is only available on Linux")

        can_socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        try:
            can_socket.bind((self.channel,))
        except Exception:
            can_socket.close()
            raise
        self._socket = can_socket

    def close(self) -> None:
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def __enter__(self) -> "SocketCanBus":
        self.open()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()

    def send(self, frame: CanFrame) -> None:
        can_socket = self._require_socket()
        payload = frame.data.ljust(8, b"\x00")
        raw_frame = struct.pack(
            CAN_FRAME_FORMAT, frame.can_id, len(frame.data), payload
        )
        can_socket.send(raw_frame)

    def receive(self, timeout: Optional[float] = None) -> Optional[CanFrame]:
        can_socket = self._require_socket()
        can_socket.settimeout(timeout)
        try:
            raw_frame = can_socket.recv(CAN_FRAME_SIZE)
        except socket.timeout:
            return None

        if len(raw_frame) != CAN_FRAME_SIZE:
            raise OSError(f"Incomplete SocketCAN frame: {len(raw_frame)} bytes")

        raw_can_id, data_length, payload = struct.unpack(
            CAN_FRAME_FORMAT, raw_frame
        )
        if raw_can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG):
            return None
        if data_length > 8:
            raise OSError(f"Invalid Classic CAN DLC: {data_length}")

        return CanFrame(raw_can_id & CAN_SFF_MASK, payload[:data_length])

    def _require_socket(self) -> socket.socket:
        if self._socket is None:
            raise RuntimeError("SocketCAN bus is not open")
        return self._socket
