"""Stage a ROM-free Windows x64 release and resolve its runtime DLL closure.

Build Release first. Requires MSYS2 objdump; never edits or deletes source/build
directories. Refuses to reuse a staging directory, preventing stale payloads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", default="build-release")
p.add_argument("--mingw", default="C:/msys64/mingw64")
a = p.parse_args()
version = (ROOT / "VERSION").read_text().strip()
if not re.fullmatch(r"\d+\.\d+\.\d+", version):
    raise SystemExit("Invalid VERSION")
build, mingw = ROOT / a.build, Path(a.mingw)
exe = build / "FZeroSNESRecomp.exe"
if version.encode() not in exe.read_bytes():
    raise SystemExit("Executable does not contain the release version")
for source in (ROOT / "src/gen").glob("*.c"):
    if "rtl_aot_node_denied(" in source.read_text():
        raise SystemExit("Regenerate without the AOT deny gate before packaging")
name = f"FZeroSNESRecomp-{version}-windows-x64"
stage = ROOT / "release-stage" / name
stage.mkdir(parents=True, exist_ok=False)
shutil.copy2(exe, stage / exe.name)
shutil.copytree(build / "assets", stage / "assets")
for filename in ("README.md", "CHANGELOG.md", "VERSION"):
    shutil.copy2(ROOT / filename, stage / filename)
(stage / "docs").mkdir()
shutil.copy2(ROOT / "docs/ADAPTIVE_RENDERER.md", stage / "docs/ADAPTIVE_RENDERER.md")
(stage / "README.txt").write_text(
    f"FZeroSNESRecomp {version} - Windows x64\n\n"
    "Extract the entire ZIP and run FZeroSNESRecomp.exe. Select your own\n"
    "F-Zero (USA) ROM in the launcher. No ROM is included.\n\n"
    "Mods contains independent Widescreen and Presentation FPS plugins.\n"
    "Enable each plugin and choose its aspect or FPS setting, then Play.\n"
    "Arrows: steer; Z: accelerate; X: A; Enter: Start.\n"
    "Ctrl+F6: aspect; Ctrl+F7: enable/cycle FPS; Alt+Enter: fullscreen.\n"
    "P: pause; Ctrl+R: reset; Shift+F1..F12: save; F1..F12: load.\n\n"
    "See CHANGELOG.md and docs/ADAPTIVE_RENDERER.md for validation limits.\n",
    encoding="utf-8")

pending, seen = [stage / exe.name], set()
system = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"
while pending:
    binary = pending.pop()
    imports = subprocess.check_output([str(mingw / "bin/objdump.exe"), "-p", str(binary)], text=True)
    for dll in re.findall(r"DLL Name:\s*(\S+)", imports):
        key = dll.lower()
        if key in seen:
            continue
        seen.add(key)
        if key.startswith(("api-ms-", "ext-ms-")) or (system / dll).is_file():
            continue
        source = next((directory / dll for directory in (build, mingw / "bin")
                       if (directory / dll).is_file()), None)
        if source is None:
            raise SystemExit(f"Unresolved runtime DLL: {dll}")
        target = stage / dll
        shutil.copy2(source, target)
        pending.append(target)

notices = stage / "licenses"
notices.mkdir()
for label, source in {
    "snesrecomp": ROOT / "snesrecomp/LICENSE",
    "recomp-ui": ROOT / "recomp-ui/LICENSE",
    "imgui": ROOT / "recomp-ui/src/third_party/imgui/LICENSE.txt",
}.items():
    shutil.copy2(source, notices / (label + ".txt"))
for package in ("gcc-libs", "libwinpthread", "winpthreads", "SDL3", "crt", "headers"):
    source = mingw / "share/licenses" / package
    if source.is_dir():
        shutil.copytree(source, notices / package)

# Preserve copyright and license records embedded in the bundled font files.
for font in (stage / "assets/fonts").glob("*.ttf"):
    data = font.read_bytes()
    records = []
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        if tag != b"name":
            continue
        _, count, strings = struct.unpack_from(">HHH", data, offset)
        for j in range(count):
            platform, _, _, name_id, length, location = struct.unpack_from(">6H", data, offset + 6 + 12*j)
            if name_id not in (0, 7, 8, 9, 13, 14):
                continue
            raw = data[offset + strings + location:offset + strings + location + length]
            text = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman", errors="replace")
            if text not in records:
                records.append(text)
    (notices / (font.stem + ".txt")).write_text("\n\n".join(records), encoding="utf-8")

git = "C:/Program Files/Git/mingw64/bin/git.exe"
commit = subprocess.check_output([git, "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
manifest = {"version": version, "commit": commit, "files": {}}
for path in sorted(stage.rglob("*")):
    if path.is_file():
        if path.suffix.lower() in (".sfc", ".smc", ".srm", ".sav", ".bin", ".c"):
            raise SystemExit(f"Forbidden payload: {path}")
        manifest["files"][path.relative_to(stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
archive = stage.parent / (stage.name + ".zip")
with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            z.write(path, path.relative_to(stage.parent))
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
print(f"{archive}\nSHA256 {digest}\nSource {commit}")
