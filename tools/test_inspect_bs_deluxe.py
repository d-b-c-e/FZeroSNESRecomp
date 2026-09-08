"""Small synthetic patch tests; no game assets required."""
import struct
import unittest
import zlib

from inspect_bs_deluxe import apply_bps, apply_ips


class PatchTests(unittest.TestCase):
    def test_bps_overlapping_target_copy_and_crc(self):
        # Empty source -> literal 'a', then copy three bytes from target offset 0.
        body = b"BPS1" + bytes([0x80, 0x84, 0x80, 0x81]) + b"a" + bytes([0x8b, 0x80])
        body += struct.pack("<II", 0, zlib.crc32(b"aaaa"))
        patch = body + struct.pack("<I", zlib.crc32(body))
        self.assertEqual(apply_bps(b"", patch), b"aaaa")
        corrupt = patch[:-1] + bytes([patch[-1] ^ 1])
        with self.assertRaises(ValueError):
            apply_bps(b"", corrupt)

    def test_ips_literal_run_and_truncation(self):
        patch = b"PATCH\x00\x00\x01\x00\x01Z\x00\x00\x03\x00\x00\x00\x03xEOF"
        self.assertEqual(apply_ips(b"abcd", patch), b"aZcxxx")
        self.assertEqual(apply_ips(b"abcd", patch + b"\x00\x00\x04"), b"aZcx")
        with self.assertRaises(ValueError):
            apply_ips(b"abcd", patch[:12])


if __name__ == "__main__":
    unittest.main()
