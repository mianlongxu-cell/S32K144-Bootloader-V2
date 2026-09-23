import binascii
import contextlib
import io
import struct
import unittest
from unittest.mock import patch
from gateway.packed_image import PackedImage, SLOT_SIZE, HEADER_OFFSET, SLOT_BASES, transfer_chunks
from gateway.flasher import FlashOptions
from gateway.flasher_v2 import V2UdsFlasher, FlashState
from gateway.uds import UdsClient, UdsError, UdsNegativeResponseError

def packed(slot=1, version=0x03000000):
    base = SLOT_BASES[slot]
    payload = bytearray(range(256))
    struct.pack_into("<II", payload, 0, 0x20006FF0, base + 129)
    fields = [0x42493256, 1, len(payload), binascii.crc32(payload) & 0xFFFFFFFF,
              version, 123, base, base + 129, 0, 0, 0, 0, 0, 0, 0, 0]
    header = bytearray(struct.pack("<16I", *fields))
    struct.pack_into("<I", header, 60, binascii.crc32(header[:60]) & 0xFFFFFFFF)
    data = bytearray(b"\xff" * SLOT_SIZE)
    data[:len(payload)] = payload
    data[HEADER_OFFSET:HEADER_OFFSET + 64] = header
    return bytes(data)

class FakeClient:
    def __init__(self, active=0, maximum=129):
        self.active = active
        self.target = 1 - active
        self.version = 0x02000000
        self.maximum = maximum
        self.erased = self.reset = self.verified = self.exited = False
        self.unlocked = False
        self.data = bytearray()
        self.counters = []
    def diagnostic_session(self, value): pass
    def security_seed(self): return 123
    def security_key(self, value): self.unlocked = True
    def read_did(self, did):
        if did == 0xF104: return struct.pack(">4B7I", 1, 1, 0, 0, 0, 0, self.active, self.target, 0, 0, 0)
        if did == 0xF105:
            states = (5, 0) if self.active == 0 else (0, 5)
            mask = 1 << self.active
            return struct.pack(">4B2B2xII2B2xII4BII", 1, mask, self.active, 2,
                               states[0], 0, self.version if self.active == 0 else 0, 1,
                               states[1], 0, self.version if self.active == 1 else 0, 1,
                               0, 255, 255, 0, 0, 0)
        if did == 0xF101: return bytes([self.active])
        if did == 0xF103: return bytes([self.target])
        if did == 0xF102: return self.version.to_bytes(4, "big")
        raise AssertionError(did)
    def routine_start(self, rid, params=b""):
        if not self.unlocked: raise UdsNegativeResponseError(0x31, 0x33)
        if rid == 0xFF00:
            assert int.from_bytes(params[:4], "big") == SLOT_BASES[self.target]
            assert int.from_bytes(params[4:], "big") == SLOT_SIZE
            self.erased = True
        elif rid == 0xFF01:
            assert self.exited
            try: PackedImage.parse(bytes(self.data))
            except ValueError as e: raise UdsNegativeResponseError(0x31, 0x72) from e
            self.verified = True
    def request_download(self, address, size):
        assert self.erased and size == SLOT_SIZE
        return self.maximum
    def transfer_data(self, seq, chunk):
        if seq != (len(self.counters) + 1) & 255:
            raise UdsNegativeResponseError(0x36, 0x73)
        assert len(chunk) <= self.maximum - 2
        self.counters.append(seq); self.data.extend(chunk)
    def request_transfer_exit(self): self.exited = True
    def ecu_reset(self):
        assert self.verified
        image = PackedImage.parse(bytes(self.data))
        self.active = image.slot; self.version = image.version
        self.reset = True

class PackedTests(unittest.TestCase):
    def test_both_slot_images(self):
        for slot in (0, 1):
            image = PackedImage.parse(packed(slot))
            self.assertEqual((image.slot, image.image_size, image.version), (slot, 256, 0x03000000))
    def test_raw_image_rejected(self):
        with self.assertRaises(ValueError): PackedImage.parse(b"12345678")
    def test_corruptions_rejected(self):
        for offset in (0, 8, HEADER_OFFSET, HEADER_OFFSET + 24, HEADER_OFFSET + 60):
            with self.subTest(offset=offset):
                image = bytearray(packed()); image[offset] ^= 1
                with self.assertRaises(ValueError): PackedImage.parse(bytes(image))
    def test_chunking_uses_advertised_size(self):
        data = bytes(range(255)) * 5
        for maximum in (3, 9, 129, 130, 256, 4095):
            chunks = list(transfer_chunks(data, maximum))
            self.assertEqual(b"".join(chunks), data)
            self.assertTrue(all(0 < len(c) <= maximum - 2 for c in chunks))
    def test_invalid_block_lengths(self):
        for maximum in (0, 1, 2, 4096):
            with self.assertRaises(ValueError): list(transfer_chunks(b"hello", maximum))

class WorkflowTests(unittest.TestCase):
    def run_flash(self, client, image, **options):
        worker = V2UdsFlasher(client, FlashOptions(**options))
        with patch("gateway.flasher_v2.time.sleep"), contextlib.redirect_stdout(io.StringIO()):
            worker.flash(image)
        return worker
    def test_a_to_b_and_b_to_a(self):
        for active in (0, 1):
            client = FakeClient(active)
            worker = self.run_flash(client, packed(1-active))
            self.assertEqual(worker.state, FlashState.DONE)
            self.assertTrue(client.reset)
            self.assertIn(0, client.counters)  # BSC rollover exercised.
    def test_wrong_slot_no_erase(self):
        client = FakeClient()
        with self.assertRaises(ValueError): self.run_flash(client, packed(0))
        self.assertFalse(client.erased)
    def test_equal_or_lower_version_no_erase(self):
        for version in (0x02000000, 0x01000000):
            client = FakeClient()
            with self.assertRaises(ValueError): self.run_flash(client, packed(version=version))
            self.assertFalse(client.erased)
    def test_address_override_must_match(self):
        client = FakeClient()
        with self.assertRaises(ValueError): self.run_flash(client, packed(), address=0)
        self.assertFalse(client.erased)
    def test_sequence_error_stops_without_reset(self):
        client = FakeClient()
        with self.assertRaises(UdsNegativeResponseError) as cm:
            self.run_flash(client, packed(), wrong_sequence=True)
        self.assertEqual(cm.exception.nrc, 0x73); self.assertFalse(client.reset)
    def test_bad_crc_stops_before_reset(self):
        client = FakeClient()
        with self.assertRaises(UdsNegativeResponseError) as cm:
            self.run_flash(client, packed(), wrong_crc=True)
        self.assertEqual(cm.exception.nrc, 0x72); self.assertFalse(client.reset)
    def test_security_denial(self):
        client = FakeClient()
        with self.assertRaises(UdsNegativeResponseError) as cm:
            self.run_flash(client, packed(), skip_security=True)
        self.assertEqual(cm.exception.nrc, 0x33); self.assertFalse(client.erased)
    def test_interruption_does_not_exit_verify_reset(self):
        client = FakeClient()
        worker = self.run_flash(client, packed(), stop_at_percent=30)
        self.assertEqual(worker.state, FlashState.INTERRUPTED)
        self.assertFalse(client.exited or client.verified or client.reset)

class PendingTests(unittest.TestCase):
    class Transport:
        def __init__(self, replies):
            self.replies = iter(replies); self.sends = 0; self.waits = []
        def request(self, data):
            self.sends += 1; return b"\x7f\x31\x78"
        def receive_response(self, timeout):
            self.waits.append(timeout); return next(self.replies)
    def test_pending_does_not_resend_request(self):
        transport = self.Transport([b"\x7f\x31\x78", b"\x71\x01\xff\x00"])
        UdsClient(transport).routine_start(0xFF00)
        self.assertEqual(transport.sends, 1)
        self.assertEqual(len(transport.waits), 2)
    def test_pending_is_bounded(self):
        transport = self.Transport([b"\x7f\x31\x78"] * 130)
        with self.assertRaises(UdsError): UdsClient(transport).routine_start(0xFF00)
        self.assertEqual(transport.sends, 1)
    def test_negative_after_pending_preserved(self):
        transport = self.Transport([b"\x7f\x31\x72"])
        with self.assertRaises(UdsNegativeResponseError) as cm:
            UdsClient(transport).routine_start(0xFF00)
        self.assertEqual(cm.exception.nrc, 0x72)

if __name__ == "__main__":
    unittest.main()
