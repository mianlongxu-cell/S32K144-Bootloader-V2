from __future__ import annotations

from collections import deque
import unittest

from gateway.can_bus import CanFrame
from gateway.isotp import IsoTpClient, IsoTpSequenceError
from gateway.uds import (
    DID_VEHICLE_SPEED,
    DtcRecord,
    UdsClient,
    UdsNegativeResponseError,
    decode_did_value,
)


class FakeBus:
    def __init__(self, received_frames) -> None:
        self.received_frames = deque(received_frames)
        self.sent_frames = []

    def send(self, frame: CanFrame) -> None:
        self.sent_frames.append(frame)

    def receive(self, timeout=None):
        if self.received_frames:
            return self.received_frames.popleft()
        return None


class IsoTpClientTests(unittest.TestCase):
    def test_multi_frame_request_honors_flow_control(self) -> None:
        payload = bytes(range(20))
        bus = FakeBus([
            CanFrame(0x7E8, bytes.fromhex("30 00 00 00 00 00 00 00")),
            CanFrame(0x7E8, bytes.fromhex("02 76 01 00 00 00 00 00")),
        ])
        response = IsoTpClient(bus, 0x7E0, 0x7E8).request(payload)
        self.assertEqual(response, bytes.fromhex("76 01"))
        self.assertEqual(bus.sent_frames[0].data[:2], bytes((0x10, 20)))
        self.assertEqual(bus.sent_frames[1].data[0], 0x21)
        self.assertEqual(bus.sent_frames[2].data[0], 0x22)

    def test_single_frame_request_and_response(self) -> None:
        bus = FakeBus(
            [
                CanFrame(0x100, bytes(8)),
                CanFrame(0x7E8, bytes.fromhex("02 7E 00 00 00 00 00 00")),
            ]
        )
        client = IsoTpClient(bus, 0x7E0, 0x7E8)

        response = client.request(bytes.fromhex("3E 00"))

        self.assertEqual(response, bytes.fromhex("7E 00"))
        self.assertEqual(len(bus.sent_frames), 1)
        self.assertEqual(bus.sent_frames[0].can_id, 0x7E0)
        self.assertEqual(
            bus.sent_frames[0].data,
            bytes.fromhex("02 3E 00 00 00 00 00 00"),
        )

    def test_vin_multi_frame_response_sends_fc_and_reassembles_cfs(self) -> None:
        uds_payload = bytes.fromhex("62 F1 90") + b"TESTS32K144000001"
        first_frame = bytes((0x10, len(uds_payload))) + uds_payload[:6]
        consecutive_one = bytes((0x21,)) + uds_payload[6:13]
        consecutive_two = bytes((0x22,)) + uds_payload[13:20]
        bus = FakeBus(
            [
                CanFrame(0x7E8, first_frame),
                CanFrame(0x7E8, consecutive_one),
                CanFrame(0x7E8, consecutive_two),
            ]
        )
        client = IsoTpClient(bus, 0x7E0, 0x7E8)

        response = client.request(bytes.fromhex("22 F1 90"))

        self.assertEqual(response, uds_payload)
        self.assertEqual(len(bus.sent_frames), 2)
        self.assertEqual(
            bus.sent_frames[1].data,
            bytes.fromhex("30 00 00 00 00 00 00 00"),
        )

    def test_wrong_consecutive_frame_sequence_is_rejected(self) -> None:
        uds_payload = bytes.fromhex("62 F1 90") + b"TESTS32K144000001"
        bus = FakeBus(
            [
                CanFrame(0x7E8, bytes((0x10, len(uds_payload))) + uds_payload[:6]),
                CanFrame(0x7E8, bytes((0x22,)) + uds_payload[6:13]),
            ]
        )

        with self.assertRaises(IsoTpSequenceError):
            IsoTpClient(bus, 0x7E0, 0x7E8).request(bytes.fromhex("22 F1 90"))


class FakeIsoTpTransport:
    def __init__(self, responses) -> None:
        self.responses = deque(responses)
        self.requests = []

    def request(self, payload: bytes) -> bytes:
        self.requests.append(payload)
        return self.responses.popleft()


class UdsClientTests(unittest.TestCase):
    def test_extended_session_timing_decodes_big_endian_fields(self) -> None:
        transport = FakeIsoTpTransport(
            [bytes.fromhex("50 03 00 32 01 F4")]
        )

        timing = UdsClient(transport).diagnostic_session(0x03)

        self.assertEqual(transport.requests[0], bytes.fromhex("10 03"))
        self.assertEqual(timing.p2_server_max_ms, 50)
        self.assertEqual(timing.p2_star_server_max_ms, 5000)

    def test_read_speed_did_and_decode_little_endian_value(self) -> None:
        transport = FakeIsoTpTransport([bytes.fromhex("62 01 01 3C 00")])
        data = UdsClient(transport).read_did(DID_VEHICLE_SPEED)

        self.assertEqual(transport.requests[0], bytes.fromhex("22 01 01"))
        self.assertEqual(decode_did_value("speed", data), "60 km/h")

    def test_read_two_dem_dtc_records_and_status_bits(self) -> None:
        transport = FakeIsoTpTransport(
            [bytes.fromhex("59 02 0D 00 01 01 0D 00 03 01 08")]
        )

        report = UdsClient(transport).read_dtc_by_status_mask()

        self.assertEqual(transport.requests[0], bytes.fromhex("19 02 FF"))
        self.assertEqual(report.status_availability_mask, 0x0D)
        self.assertEqual(
            report.records,
            [DtcRecord(0x000101, 0x0D), DtcRecord(0x000301, 0x08)],
        )
        self.assertEqual(
            report.records[0].status_names,
            ["testFailed", "pendingDTC", "confirmedDTC"],
        )

    def test_negative_response_exposes_nrc(self) -> None:
        transport = FakeIsoTpTransport([bytes.fromhex("7F 22 31")])

        with self.assertRaises(UdsNegativeResponseError) as context:
            UdsClient(transport).read_did(0xAAAA)

        self.assertEqual(context.exception.request_sid, 0x22)
        self.assertEqual(context.exception.nrc, 0x31)


if __name__ == "__main__":
    unittest.main()
