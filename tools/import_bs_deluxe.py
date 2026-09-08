"""Local all-or-nothing BS Deluxe conversion. Requires the user's archive/ROM.

Produces a private guarded cartridge delta and native-module namespace header.
No installed patched ROM is required. Generated output must not be committed.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import zipfile

from inspect_bs_deluxe import STOCK_SHA256, apply_bps, apply_ips, sha

DELUXE_SHA256 = "77bb37bcdedd3e17321727d5ed6a14792aa7a45ac04e9040b16bf564bd6dea24"


def namespace(gen):
    symbols = set()
    for path in gen.glob("*.c"):
        symbols.update(re.findall(r"^(?:RecompReturn|void)\s+(\w+)\s*\(CpuState\s*\*cpu\)", path.read_text(), re.M))
    if not symbols:
        raise ValueError("Generate the Deluxe native sources before creating its namespace")
    symbols.update(("g_dispatch_table", "g_dispatch_table_count", "g_ram_routine_guards", "g_ram_routine_guard_count"))
    (gen / "deluxe_namespace.h").write_text(
        "/* Generated compile-time namespace; definitions AND direct calls. */\n" +
        "".join(f"#define {name} deluxe_{name}\n" for name in sorted(symbols)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--gen", type=Path, required=True)
    a = p.parse_args()
    source = a.stock.read_bytes()
    if len(source) == 524800:
        source = source[512:]
    if sha(source) != STOCK_SHA256:
        raise ValueError("Unsupported stock ROM")
    with zipfile.ZipFile(a.archive) as z:
        target = apply_bps(source, z.read("patches/bs-deluxe-usa.bps"))
        if target != apply_ips(source, z.read("patches/bs-deluxe-usa.ips")):
            raise ValueError("Patch formats disagree")
        credits = z.read("readme.txt")
    if sha(target) != DELUXE_SHA256:
        raise ValueError("Unsupported Deluxe revision: native module is pinned to USA 1.0")
    baseline = source + bytes(len(target) - len(source))
    records = []
    start = None
    for i, (old, new) in enumerate(zip(baseline, target)):
        if old != new and start is None:
            start = i
        if old == new and start is not None:
            records.append((start, target[start:i]))
            start = None
    if start is not None:
        records.append((start, target[start:]))
    payload = b"BSDELX1\0" + struct.pack("<II", len(target), len(records))
    payload += bytes.fromhex(STOCK_SHA256) + bytes.fromhex(DELUXE_SHA256)
    for offset, data in records:
        payload += struct.pack("<II", offset, len(data)) + data
    namespace(a.gen)
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / "bs-deluxe.dat").write_bytes(payload)
    (a.out / "BS-Deluxe-credits.txt").write_bytes(credits)
    (a.out / "bs-deluxe-import.json").write_text(json.dumps({
        "format": 1, "package": "bs-deluxe", "version": "1.0-USA",
        "ownership": "Entire Deluxe cartridge delta; indivisible launch-time feature",
        "archive_sha256": sha(a.archive.read_bytes()), "stock_sha256": sha(source),
        "target_sha256": sha(target), "delta_sha256": sha(payload),
        "records": len(records), "payload_bytes": len(payload),
    }, indent=2) + "\n")
    print(f"Imported {len(records)} guarded delta records ({len(payload)} bytes); native namespace ready")


if __name__ == "__main__":
    main()
