from __future__ import annotations

import unittest

from gateway.flasher import calculate_key, crc32


class FlasherHelpersTests(unittest.TestCase):
    def test_crc32_matches_standard_vector(self) -> None:
        self.assertEqual(crc32(b"123456789"), 0xCBF43926)

    def test_seed_key_transform(self) -> None:
        seed = 0x12345678
        mixed = (seed ^ 0xA5C39E71) & 0xFFFFFFFF
        expected = ((mixed << 7) | (mixed >> 25)) & 0xFFFFFFFF
        self.assertEqual(calculate_key(seed), expected)


if __name__ == "__main__":
    unittest.main()
