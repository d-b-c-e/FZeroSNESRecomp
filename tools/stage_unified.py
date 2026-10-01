"""Create a fresh explicit stock-only staging folder and build receipt.

Run immediately after a clean source build. Reads only that build and its
verified dependency roots; never launches binaries, copies configs or ROMs,
or modifies a source/install folder. Use package_unified.py afterward.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess

from package_unified import ROOT, UI_ASSETS, SYSTEM_DLLS, digest, pe_imports


def git(root, *args):
    return subprocess.check_output(["git", "-c", "safe.directory=" + root.as_posix(),
                                   "-C", str(root), *args], text=True).strip()


def font_notice(data):
    records = []
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        if tag != b"name":
            continue
        _, count, strings = struct.unpack_from(">HHH", data, offset)
        for j in range(count):
            platform, _, _, name_id, length, location = struct.unpack_from(">6H", data, offset + 6 + 12 * j)
            if name_id not in (0, 7, 8, 9, 13, 14):
                continue
            raw = data[offset + strings + location:offset + strings + location + length]
            text = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman")
            if text not in records:
                records.append(text)
    if not records:
        raise ValueError("Font has no embedded copyright/license records")
    return ("\n\n".join(records) + "\n").encode()


def stage(build, output):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)", line)
        if match:
            cache[match[1]] = match[2]
    if cache.get("FZERO_DELUXE_GEN_DIR") or cache.get("FZERO_DELUXE_DATA_FILE"):
        raise ValueError("Stock-only build required; BS-generated module excluded")
    revision, tree = git(ROOT, "rev-parse", "HEAD"), git(ROOT, "rev-parse", "HEAD^{tree}")
    if git(ROOT, "status", "--porcelain", "--untracked-files=no"):
        raise ValueError("Commit tested source before the release build")
    header = (build / "generated/fzero_build.h").read_text()
    description = git(ROOT, "describe", "--always", "--dirty", "--abbrev=12")
    if f'#define FZERO_SOURCE_REVISION "{description}"' not in header:
        raise ValueError("Build source stamp is stale or dirty; reconfigure/rebuild")
    version = (ROOT / "VERSION").read_text().strip()
    files = {"FZeroSNESRecomp.exe": (build / "FZeroSNESRecomp.exe").read_bytes()}
    if version.encode() not in files["FZeroSNESRecomp.exe"]:
        raise ValueError("Build does not contain product version")
    if b"BSDELX1" in files["FZeroSNESRecomp.exe"]:
        raise ValueError("Embedded private BS module excluded")
    links = dict((line.split()[3], line.split()[1]) for line in git(ROOT, "ls-files", "--stage", "snesrecomp", "recomp-ui").splitlines())
    dependencies = {}
    for name, key in (("snesrecomp", "SNESRECOMP_ROOT"), ("recomp-ui", "RECOMP_UI_ROOT")):
        root = Path(cache[key]).resolve()
        if git(root, "rev-parse", "HEAD") != links[name] or git(root, "status", "--porcelain", "--untracked-files=no"):
            raise ValueError(f"Dependency {name} does not match clean gitlink")
        dependencies[name] = root
        files[f"licenses/{name}.txt"] = (root / "LICENSE").read_bytes()
    files["licenses/imgui.txt"] = (dependencies["recomp-ui"] / "src/third_party/imgui/LICENSE.txt").read_bytes()
    files["licenses/wheel-toolkit.txt"] = (ROOT / "lib/toolkit/LICENSE.txt").read_bytes()
    files["WheelFfb.dll"] = (ROOT / "lib/toolkit/native/WheelFfb.dll").read_bytes()
    for name in UI_ASSETS:
        files[name] = (build / name).read_bytes()
        if name.endswith(".ttf"):
            source = dependencies["recomp-ui"] / "assets/common/fonts" / Path(name).name
            if files[name] != source.read_bytes():
                raise ValueError("Unpinned launcher font")
            files["licenses/" + Path(name).stem + ".txt"] = font_notice(files[name])
        elif name != "assets/img/boxart.tga":
            relative = "assets/consoles/snes/img/pad.tga" if name.endswith("/pad.tga") else "assets/common/img/" + Path(name).name
            if files[name] != (dependencies["recomp-ui"] / relative).read_bytes():
                raise ValueError("Unpinned launcher image")
        elif files[name] != (ROOT / name).read_bytes():
            raise ValueError("Unpinned box art")
    for name in git(ROOT, "ls-files", "assets/shaders").splitlines():
        files[name] = (ROOT / name).read_bytes()
    pending = ["FZeroSNESRecomp.exe", "WheelFfb.dll"]
    while pending:
        name = pending.pop()
        for imported in pe_imports(files[name]):
            if imported in SYSTEM_DLLS or imported.startswith(("api-ms-", "ext-ms-")):
                continue
            existing = next((n for n in files if n.lower() == imported), None)
            if existing:
                continue
            source = next((p for p in build.glob("*.dll") if p.name.lower() == imported), None)
            if source is None:
                raise ValueError(f"Runtime import absent from build: {imported}")
            # Extra third-party runtimes require reviewed notices; don't infer terms.
            notice = build / "licenses" / (source.stem + ".txt")
            files["licenses/" + source.stem + ".txt"] = notice.read_bytes()
            files[source.name] = source.read_bytes()
            pending.append(source.name)
    receipt = {"schema": "dbce.fzero-build", "version": 1, "productVersion": version,
               "sourceRevision": revision, "sourceTree": tree, "dirty": False,
               "bsDeluxe": False, "dependencies": links,
               "files": {n: digest(d) for n, d in sorted(files.items())}}
    output.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        path = output / "payload" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (output / "build-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return output / "build-receipt.json"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(stage(args.build.resolve(), args.output.resolve()))
    except (ValueError, OSError, KeyError, struct.error, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
