from __future__ import annotations

from datetime import datetime
from pathlib import Path
import struct
import tempfile
import unittest

from gateway.can_bus import CAN_FRAME_FORMAT, CanFrame, SocketCanBus
from gateway.csv_logger import CSV_FIELDS, CsvStateLogger
from gateway.vehicle import VehicleState, build_control_frame


class VehicleDecoderTests(unittest.TestCase):
    def test_powertrain_frame_decodes_little_endian_signals(self) -> None:
        state = VehicleState()
        recognized = state.update(
            CanFrame(0x100, bytes.fromhex("3C 00 DC 05 55 03 00 00"))
        )

        self.assertTrue(recognized)
        self.assertEqual(state.powertrain.speed_kmh, 60)
        self.assertEqual(state.powertrain.rpm, 1500)
        self.assertEqual(state.powertrain.coolant_c, 85)
        self.assertEqual(state.powertrain.gear, 3)

    def test_body_frame_decodes_all_five_signals(self) -> None:
        state = VehicleState()
        state.update(CanFrame(0x101, bytes.fromhex("01 01 00 02 01 00 00 00")))

        self.assertEqual(state.body.light, 1)
        self.assertEqual(state.body.door, 1)
        self.assertEqual(state.body.wiper, 0)
        self.assertEqual(state.body.turn_signal, 2)
        self.assertEqual(state.body.lock, 1)

    def test_fault_frame_decodes_legacy_low_16_bit_dtc(self) -> None:
        state = VehicleState()
        state.update(CanFrame(0x102, bytes.fromhex("01 01 01 03 02 0D 00 00")))

        self.assertEqual(state.fault.active, 1)
        self.assertEqual(state.fault.dtc, 0x0101)
        self.assertEqual(state.fault.level, 3)
        self.assertEqual(state.fault.occurrence, 2)
        self.assertEqual(state.fault.status, 0x0D)

    def test_unknown_frame_is_ignored(self) -> None:
        self.assertFalse(VehicleState().update(CanFrame(0x555, b"\x00")))

    def test_short_configured_frame_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            VehicleState().update(CanFrame(0x100, b"\x00" * 6))


class ControlEncoderTests(unittest.TestCase):
    def test_overtemp_command_matches_control_protocol(self) -> None:
        frame = build_control_frame("overtemp")
        self.assertEqual(frame.can_id, 0x200)
        self.assertEqual(frame.data, bytes.fromhex("04 00 00 00 00 00 00 00"))

    def test_turn_command_carries_argument_in_byte_one(self) -> None:
        frame = build_control_frame("turn", 2)
        self.assertEqual(frame.data, bytes.fromhex("0A 02 00 00 00 00 00 00"))

    def test_heartbeat_has_dedicated_command_value(self) -> None:
        frame = build_control_frame("heartbeat")
        self.assertEqual(frame.data, bytes.fromhex("0B 00 00 00 00 00 00 00"))

    def test_invalid_turn_value_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            build_control_frame("turn", 4)


class FakeCanSocket:
    def __init__(self, receive_data: bytes = b"") -> None:
        self.receive_data = receive_data
        self.sent_data = b""
        self.timeout = None

    def send(self, data: bytes) -> None:
        self.sent_data = data

    def recv(self, size: int) -> bytes:
        return self.receive_data[:size]

    def settimeout(self, timeout) -> None:
        self.timeout = timeout

    def close(self) -> None:
        pass


class SocketCanEncodingTests(unittest.TestCase):
    def test_send_encodes_linux_can_frame_layout(self) -> None:
        fake_socket = FakeCanSocket()
        bus = SocketCanBus("can0")
        bus._socket = fake_socket

        bus.send(CanFrame(0x200, bytes.fromhex("04 00 00 00 00 00 00 00")))
        can_id, data_length, payload = struct.unpack(
            CAN_FRAME_FORMAT, fake_socket.sent_data
        )

        self.assertEqual(can_id, 0x200)
        self.assertEqual(data_length, 8)
        self.assertEqual(payload, bytes.fromhex("04 00 00 00 00 00 00 00"))

    def test_receive_decodes_linux_can_frame_layout(self) -> None:
        raw_frame = struct.pack(
            CAN_FRAME_FORMAT,
            0x100,
            8,
            bytes.fromhex("3C 00 DC 05 55 03 00 00"),
        )
        bus = SocketCanBus("can0")
        bus._socket = FakeCanSocket(raw_frame)

        frame = bus.receive(timeout=0.25)

        self.assertEqual(frame, CanFrame(0x100, bytes.fromhex("3C 00 DC 05 55 03 00 00")))


class CsvLoggerTests(unittest.TestCase):
    def test_logger_writes_header_and_snapshot(self) -> None:
        state = VehicleState()
        state.update(CanFrame(0x100, bytes.fromhex("3C 00 DC 05 55 03 00 00")))
        state.update(CanFrame(0x102, bytes.fromhex("01 01 01 03 02 0D 00 00")))

        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "vehicle.csv"
            with CsvStateLogger(str(output)) as logger:
                logger.write(state, datetime(2026, 8, 21, 12, 30, 1, 100000))

            rows = output.read_text(encoding="utf-8").splitlines()

        self.assertEqual(rows[0].split(","), list(CSV_FIELDS))
        self.assertIn("2026-08-21T12:30:01.100,60,1500,85,3", rows[1])
        self.assertIn(",1,000101,3,2,0D", rows[1])


if __name__ == "__main__":
    unittest.main()
