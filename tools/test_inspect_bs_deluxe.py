"""Small synthetic patch tests; no game assets required."""
import io
import struct
import unittest
import zipfile
import zlib

from inspect_bs_deluxe import apply_bps, apply_ips, readme_version, usa_patch_names


def _zip(members):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as z:
        for name, data in members.items():
            z.writestr(name, data)
    buffer.seek(0)
    return zipfile.ZipFile(buffer)


class ArchiveLayoutTests(unittest.TestCase):
    def test_v10_and_v11_patch_names(self):
        v10 = _zip({"patches/bs-deluxe-usa.bps": b"", "patches/bs-deluxe-usa.ips": b"",
                    "patches/bs-deluxe-eur.bps": b"", "readme.txt": "﻿BS F-Zero Deluxe - v1.0\n"})
        self.assertEqual(usa_patch_names(v10), ("patches/bs-deluxe-usa.bps", "patches/bs-deluxe-usa.ips"))
        self.assertEqual(readme_version(v10), "1.0")
        v11 = _zip({"patches/bs-deluxe-v1.1-usa.bps": b"", "patches/bs-deluxe-v1.1-usa.ips": b"",
                    "patches/bs-deluxe-v1.1-jpn.ips": b"", "readme.txt": "﻿BS F-Zero Deluxe - v1.1\n"})
        self.assertEqual(usa_patch_names(v11), ("patches/bs-deluxe-v1.1-usa.bps", "patches/bs-deluxe-v1.1-usa.ips"))
        self.assertEqual(readme_version(v11), "1.1")

    def test_ambiguous_or_missing_usa_pair_rejected(self):
        with self.assertRaises(ValueError):
            usa_patch_names(_zip({"patches/bs-deluxe-usa.bps": b"", "patches/bs-deluxe-v1.1-usa.bps": b"",
                                  "patches/bs-deluxe-usa.ips": b""}))
        with self.assertRaises(ValueError):
            usa_patch_names(_zip({"patches/bs-deluxe-eur.bps": b"", "patches/bs-deluxe-eur.ips": b""}))
        with self.assertRaises(ValueError):
            readme_version(_zip({"readme.txt": "BS F-Zero Deluxe\n"}))


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
