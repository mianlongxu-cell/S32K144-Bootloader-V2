"""Minimal ISO-TP client for the Vehicle ECU's physical diagnostic channel."""

from __future__ import annotations

import time
from typing import Optional, Protocol

from .can_bus import CanFrame


PCI_TYPE_SINGLE_FRAME = 0x0
PCI_TYPE_FIRST_FRAME = 0x1
PCI_TYPE_CONSECUTIVE_FRAME = 0x2
PCI_TYPE_FLOW_CONTROL = 0x3

FLOW_STATUS_CONTINUE_TO_SEND = 0x0


class CanTransport(Protocol):
    def send(self, frame: CanFrame) -> None:
        ...

    def receive(self, timeout: Optional[float] = None) -> Optional[CanFrame]:
        ...


class IsoTpError(RuntimeError):
    """Base error raised for malformed frames or transport timeouts."""


class IsoTpTimeoutError(IsoTpError):
    pass


class IsoTpSequenceError(IsoTpError):
    pass


class IsoTpClient:
    """ISO-TP client supporting SF/FF/CF/FC in both directions."""

    def __init__(
        self,
        bus: CanTransport,
        request_id: int,
        response_id: int,
        response_timeout: float = 1.0,
        consecutive_frame_timeout: float = 1.0,
    ) -> None:
        if response_timeout <= 0 or consecutive_frame_timeout <= 0:
            raise ValueError("ISO-TP timeouts must be greater than zero")
        self._bus = bus
        self.request_id = request_id
        self.response_id = response_id
        self.response_timeout = response_timeout
        self.consecutive_frame_timeout = consecutive_frame_timeout

    def request(self, payload: bytes) -> bytes:#发送一条 UDS 请求
        if not 1 <= len(payload) <= 0xFFF:
            raise ValueError("ISO-TP payload length must be in range 1..4095")

        if len(payload) <= 7:
            self._send_single_frame(payload)
        else:
            self._send_multi_frame(payload)
        first_response = self._receive_response(self.response_timeout)
        return self._decode_response(first_response)

    def _send_single_frame(self, payload: bytes) -> None:
        frame = bytes((len(payload),)) + payload
        self._bus.send(CanFrame(self.request_id, frame.ljust(8, b"\x00")))

    def receive_response(self, timeout: float) -> bytes:
        """Receive final UDS response after 0x78 without resending the request."""
        return self._decode_response(self._receive_response(timeout))

    def _send_multi_frame(self, payload: bytes) -> None:
        first = bytes((0x10 | ((len(payload) >> 8) & 0x0F),
                       len(payload) & 0xFF)) + payload[:6]
        self._bus.send(CanFrame(self.request_id, first))
        flow_control = self._receive_response(self.response_timeout)
        if not flow_control.data or (flow_control.data[0] >> 4) != PCI_TYPE_FLOW_CONTROL:
            raise IsoTpError("Expected Flow Control after First Frame")
        flow_status = flow_control.data[0] & 0x0F
        if flow_status != FLOW_STATUS_CONTINUE_TO_SEND:
            raise IsoTpError(f"Flow Control rejected transfer: FS={flow_status}")
        block_size = flow_control.data[1]
        stmin = self._decode_stmin(flow_control.data[2])
        offset = 6
        sequence = 1
        block_count = 0
        while offset < len(payload):
            chunk = payload[offset : offset + 7]
            frame = bytes((0x20 | sequence,)) + chunk
            if stmin:
                time.sleep(stmin)
            self._bus.send(CanFrame(self.request_id, frame.ljust(8, b"\x00")))
            offset += len(chunk)
            sequence = (sequence + 1) & 0x0F
            block_count += 1
            if block_size and block_count >= block_size and offset < len(payload):
                flow_control = self._receive_response(self.response_timeout)
                if (not flow_control.data or
                        (flow_control.data[0] >> 4) != PCI_TYPE_FLOW_CONTROL or
                        (flow_control.data[0] & 0x0F) != FLOW_STATUS_CONTINUE_TO_SEND):
                    raise IsoTpError("Invalid repeated Flow Control")
                block_size = flow_control.data[1]
                stmin = self._decode_stmin(flow_control.data[2])
                block_count = 0

    @staticmethod
    def _decode_stmin(value: int) -> float:
        if value <= 0x7F:
            return value / 1000.0
        if 0xF1 <= value <= 0xF9:
            return (value - 0xF0) / 10000.0
        raise IsoTpError(f"Reserved STmin value: 0x{value:02X}")

    def _decode_response(self, frame: CanFrame) -> bytes:
        if len(frame.data) == 0:
            raise IsoTpError("Received an empty diagnostic CAN frame")

        pci_type = frame.data[0] >> 4
        if pci_type == PCI_TYPE_SINGLE_FRAME:
            return self._decode_single_frame(frame.data)
        if pci_type == PCI_TYPE_FIRST_FRAME:
            return self._receive_multi_frame(frame.data)
        raise IsoTpError(f"Unexpected first response PCI type: 0x{pci_type:X}")

    @staticmethod
    def _decode_single_frame(data: bytes) -> bytes:
        payload_length = data[0] & 0x0F
        if payload_length == 0 or payload_length > 7:
            raise IsoTpError(f"Invalid Single Frame length: {payload_length}")
        if len(data) < payload_length + 1:
            raise IsoTpError("Single Frame payload is shorter than its PCI length")
        return data[1 : payload_length + 1]

    def _receive_multi_frame(self, first_frame_data: bytes) -> bytes:#接收多帧响应
        if len(first_frame_data) < 8:
            raise IsoTpError("First Frame must contain a complete 8-byte CAN payload")

        total_length = ((first_frame_data[0] & 0x0F) << 8) | first_frame_data[1]
        if total_length <= 7:
            raise IsoTpError(f"Invalid First Frame PDU length: {total_length}")

        assembled = bytearray(first_frame_data[2:8])#接收多帧响应
        self._send_flow_control()
        expected_sequence_number = 1

        while len(assembled) < total_length:
            frame = self._receive_response(self.consecutive_frame_timeout)
            if len(frame.data) == 0:
                raise IsoTpError("Received an empty Consecutive Frame")
            pci_type = frame.data[0] >> 4
            if pci_type != PCI_TYPE_CONSECUTIVE_FRAME:
                raise IsoTpError(
                    f"Expected Consecutive Frame, received PCI type 0x{pci_type:X}"
                )

            sequence_number = frame.data[0] & 0x0F
            if sequence_number != expected_sequence_number:
                raise IsoTpSequenceError(
                    f"Expected CF sequence {expected_sequence_number:X}, "
                    f"received {sequence_number:X}"
                )
            if len(frame.data) < 2:
                raise IsoTpError("Consecutive Frame contains no payload")

            assembled.extend(frame.data[1:])
            expected_sequence_number = (expected_sequence_number + 1) & 0x0F

        return bytes(assembled[:total_length])

    def _send_flow_control(self) -> None:#发送流控帧
        flow_control = bytes(
            (
                (PCI_TYPE_FLOW_CONTROL << 4) | FLOW_STATUS_CONTINUE_TO_SEND,
                0x00,  # Block Size: remaining CFs may be sent without another FC.
                0x00,  # STmin: no additional delay requested.
            )
        ).ljust(8, b"\x00")
        self._bus.send(CanFrame(self.request_id, flow_control))

    def _receive_response(self, timeout: float) -> CanFrame:#等待指定响应 ID
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise IsoTpTimeoutError(
                    f"Timed out waiting for CAN ID 0x{self.response_id:03X}"
                )

            frame = self._bus.receive(timeout=remaining)
            if frame is None:
                if time.monotonic() >= deadline:
                    raise IsoTpTimeoutError(
                        f"Timed out waiting for CAN ID 0x{self.response_id:03X}"
                    )
                continue
            if frame.can_id == self.response_id:
                return frame
