"""Regenerate the private BS Deluxe native module from the pinned USA 1.1 IPS.

The tracked patch and the user's verified stock ROM are both required. ROM,
patched oracle, generated code and cartridge delta remain under ignored work/.
This is an alternative to regen_bs_deluxe.py when the original archive is no
longer available; it verifies the exact pinned target and delta hashes before
publishing either derived file.
"""

import argparse
from pathlib import Path
import subprocess
import sys

from import_bs_deluxe import DELUXE_SHA256, guarded_delta, namespace
from inspect_bs_deluxe import STOCK_SHA256, apply_ips, sha

ROOT = Path(__file__).resolve().parents[1]
PATCH_SHA256 = "2f0217a96209b5d9fd3f0f2ed348086fdc5002a478a9557d3fd522ff5fddd2f6"
DELTA_SHA256 = "a9e31fd3a32103cc6591e70c31e4a182b631989b7ab31b21cb5aec6c5c1bc02b"


def verified_sources(stock_path, patch_path):
    source = stock_path.read_bytes()
    if len(source) == 524800:
        source = source[512:]
    if len(source) != 0x80000 or sha(source) != STOCK_SHA256:
        raise ValueError("stock ROM does not match the pinned F-Zero USA revision")
    patch = patch_path.read_bytes()
    if sha(patch) != PATCH_SHA256:
        raise ValueError("IPS does not match the pinned BS Deluxe USA 1.1 patch")
    target = apply_ips(source, patch)
    if len(target) != 0x100000 or sha(target) != DELUXE_SHA256:
        raise ValueError("IPS did not produce the pinned BS Deluxe USA 1.1 target")
    payload, record_count = guarded_delta(source, target)
    if sha(payload) != DELTA_SHA256 or record_count != 47168:
        raise ValueError("guarded delta differs from the archived import receipt")
    return target, payload


def publish_exact(path, data):
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f"existing private artifact differs: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--patch", type=Path,
                   default=ROOT / "patches" / "bs-deluxe-usa.ips")
    p.add_argument("--work", type=Path,
                   default=ROOT / "captures" / "bs-deluxe")
    p.add_argument("--analysis-backend", choices=("native", "python"),
                   default="native")
    a = p.parse_args()
    target, payload = verified_sources(a.stock, a.patch)
    work = a.work.resolve()
    if work == ROOT.resolve() or ROOT.resolve() not in work.parents:
        raise ValueError("private work directory must be below this repository")
    oracle = work / "oracle.sfc"
    delta = work / "mods" / "bs-deluxe.dat"
    publish_exact(oracle, target)
    cfg = work / "cfg"
    cfg.mkdir(parents=True, exist_ok=True)
    for source_cfg in (ROOT / "recomp").glob("*.cfg"):
        publish_exact(cfg / source_cfg.name, source_cfg.read_bytes())
    command = [sys.executable, str(ROOT / "snesrecomp/tools/v2_emit.py"),
               "--rom", str(oracle), "--cfg-dir", str(cfg),
               "--out-dir", str(work / "gen"), "--cfg-roots",
               "--no-host-root-scan", "--analysis-backend", a.analysis_backend]
    subprocess.run(command, cwd=ROOT, check=True)
    namespace(work / "gen")
    publish_exact(delta, payload)
    print(f"BS Deluxe private module ready: gen={work / 'gen'} delta={delta}")


if __name__ == "__main__":
    main()
