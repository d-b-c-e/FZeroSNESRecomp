"""Audit user-supplied BS Deluxe patches; patched images are private oracles only.

Independently applies USA IPS and BPS, verifies all BPS CRCs, and reports changed
ranges, cartridge growth and potential AOT collisions. A byte diff is evidence,
not a feature ownership map or an installable mod.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib

STOCK_SHA256 = "bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2"
LIMIT = 16 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def apply_bps(source, patch):
    if len(patch) < 16 or patch[:4] != b"BPS1":
        raise ValueError("Invalid BPS header")
    source_crc, target_crc, patch_crc = struct.unpack("<III", patch[-12:])
    if zlib.crc32(patch[:-4]) != patch_crc or zlib.crc32(source) != source_crc:
        raise ValueError("BPS source or patch CRC mismatch")
    cursor, end = 4, len(patch) - 12

    def number():
        nonlocal cursor
        value, shift = 0, 1
        for _ in range(10):
            if cursor >= end:
                raise ValueError("Truncated BPS number")
            byte = patch[cursor]
            cursor += 1
            value += (byte & 127) * shift
            if byte & 128:
                return value
            shift <<= 7
            value += shift
        raise ValueError("Oversized BPS number")

    source_size, target_size, metadata_size = number(), number(), number()
    if source_size != len(source) or target_size > LIMIT or cursor + metadata_size > end:
        raise ValueError("Invalid BPS sizes")
    cursor += metadata_size
    target = bytearray()
    source_relative = target_relative = 0
    while len(target) < target_size:
        action = number()
        mode, length = action & 3, (action >> 2) + 1
        if len(target) + length > target_size:
            raise ValueError("BPS target overrun")
        if mode == 0:
            offset = len(target)
            if offset + length > len(source):
                raise ValueError("BPS source-read overrun")
            target.extend(source[offset:offset+length])
        elif mode == 1:
            if cursor + length > end:
                raise ValueError("BPS literal overrun")
            target.extend(patch[cursor:cursor+length])
            cursor += length
        else:
            encoded = number()
            delta = -(encoded >> 1) if encoded & 1 else encoded >> 1
            if mode == 2:
                source_relative += delta
                if source_relative < 0 or source_relative + length > len(source):
                    raise ValueError("BPS source-copy overrun")
                target.extend(source[source_relative:source_relative+length])
                source_relative += length
            else:
                target_relative += delta
                for _ in range(length):
                    if not 0 <= target_relative < len(target):
                        raise ValueError("BPS target-copy overrun")
                    target.append(target[target_relative])
                    target_relative += 1
    if cursor != end or zlib.crc32(target) != target_crc:
        raise ValueError("BPS target CRC or stream length mismatch")
    return bytes(target)


def apply_ips(source, patch):
    if patch[:5] != b"PATCH":
        raise ValueError("Invalid IPS header")
    target, cursor = bytearray(source), 5
    while patch[cursor:cursor+3] != b"EOF":
        if cursor + 5 > len(patch):
            raise ValueError("Truncated IPS record")
        offset = int.from_bytes(patch[cursor:cursor+3], "big")
        size = int.from_bytes(patch[cursor+3:cursor+5], "big")
        cursor += 5
        if size:
            if cursor + size > len(patch):
                raise ValueError("Truncated IPS literal")
            data = patch[cursor:cursor+size]
            cursor += size
        else:
            if cursor + 3 > len(patch):
                raise ValueError("Truncated IPS run")
            size = int.from_bytes(patch[cursor:cursor+2], "big")
            data = patch[cursor+2:cursor+3] * size
            cursor += 3
        if offset + size > LIMIT:
            raise ValueError("IPS target overrun")
        if offset + size > len(target):
            target.extend(bytes(offset + size - len(target)))
        target[offset:offset+size] = data
    cursor += 3
    if len(patch) - cursor == 3:
        size = int.from_bytes(patch[cursor:], "big")
        if size > LIMIT:
            raise ValueError("IPS truncate size overrun")
        target = target[:size] + bytes(max(0, size - len(target)))
    elif cursor != len(patch):
        raise ValueError("Trailing IPS data")
    return bytes(target)


def header(data):
    h = data[0x7fc0:0x8000]
    return {"title": h[:21].decode("ascii", errors="replace").rstrip(),
            "mapping": h[21], "cartridge_type": h[22],
            "rom_bytes": 1024 << h[23], "sram_bytes": (1024 << h[24]) if h[24] else 0,
            "reset_vector": f"{int.from_bytes(h[60:62], 'little'):04x}"}


def analyze(source, target, manifest=None):
    changed = [i for i, (a, b) in enumerate(zip(source, target)) if a != b]
    ranges = []
    for offset in changed:
        if ranges and offset == ranges[-1][1]:
            ranges[-1][1] += 1
        else:
            ranges.append([offset, offset + 1])
    collisions = []
    if manifest:
        for name, node in manifest["nodes"].items():
            if node["disposition"] == "lle_only":
                continue
            if node["disposition"] != "aot_eligible":
                raise ValueError("Unknown manifest disposition")
            lo, hi = node["min_pc24"], node["max_pc24"]
            if lo >> 16 != hi >> 16 or (lo & 65535) < 32768:
                continue
            start = ((lo >> 16) & 127) * 32768 + (lo & 32767)
            # Include a maximum-length instruction at the last opcode. This
            # deliberately over-approximates; it cannot prove semantic ownership.
            stop = ((hi >> 16) & 127) * 32768 + (hi & 32767) + 4
            if any(a < stop and b > start for a, b in ranges):
                collisions.append(name)
    hooks = {}
    for address in (0x81de, 0xc339, 0xdadc, 0xdbc4, 0xdcc6, 0xf468):
        offset = address - 0x8000
        hooks[f"00:{address:04x}"] = source[offset:offset+16] == target[offset:offset+16]
    return {"stock_header": header(source), "deluxe_header": header(target),
            "changed_stock_bytes": len(changed), "added_bytes": len(target)-len(source),
            "changed_stock_bytes_by_bank": {f"{bank:02x}": count for bank, count in sorted(Counter(i//32768 for i in changed).items())},
            "changed_stock_ranges": [{"offset": a, "length": b-a, "owner": "unreviewed"} for a, b in ranges],
            "potential_stock_aot_collisions": collisions,
            "renderer_hook_first_16_bytes_unchanged": hooks}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--manifest", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--oracle", type=Path, help="Optional private patched reference, never a runtime input")
    a = p.parse_args()
    source = a.stock.read_bytes()
    if len(source) == 524800:
        source = source[512:]
    if sha(source) != STOCK_SHA256:
        raise ValueError("Unsupported stock USA ROM")
    archive = a.archive.read_bytes()
    with zipfile.ZipFile(a.archive) as z:
        bps, ips = z.read("patches/bs-deluxe-usa.bps"), z.read("patches/bs-deluxe-usa.ips")
    target = apply_bps(source, bps)
    if target != apply_ips(source, ips):
        raise ValueError("Independent IPS and BPS results disagree")
    report = analyze(source, target, json.loads(a.manifest.read_text()) if a.manifest else None)
    report.update(archive_sha256=sha(archive), stock_sha256=sha(source),
                  bps_sha256=sha(bps), ips_sha256=sha(ips), oracle_sha256=sha(target),
                  ips_bps_agree=True, bps_crcs_verified=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if a.oracle:
        a.oracle.parent.mkdir(parents=True, exist_ok=True)
        with a.oracle.open("xb") as f:
            f.write(target)
    print(json.dumps({k: v for k, v in report.items() if k != "changed_stock_ranges"}, indent=2))


if __name__ == "__main__":
    main()
