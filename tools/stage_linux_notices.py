"""Add project/dependency notices and user instructions to a fresh AppDir."""
import argparse
import os
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("appdir", type=Path)
args = parser.parse_args()
docs = args.appdir / "usr/share/doc/fzerosnesrecomp"
notices = docs / "licenses"
notices.mkdir(parents=True, exist_ok=True)
for filename in ("LICENSE", "README.md", "CHANGELOG.md", "VERSION"):
    shutil.copy2(ROOT / filename, docs / filename)
(docs / "docs").mkdir()
shutil.copy2(ROOT / "docs/HD_MODE7.md", docs / "docs/HD_MODE7.md")
shutil.copy2(ROOT / "docs/HD_MODE7_PERFORMANCE.md", docs / "docs/HD_MODE7_PERFORMANCE.md")
shutil.copy2(ROOT / "docs/PERFORMANCE_DIAGNOSTICS.md", docs / "docs/PERFORMANCE_DIAGNOSTICS.md")
for name, source in {
    "snesrecomp": ROOT / "snesrecomp/LICENSE",
    "recomp-ui": ROOT / "recomp-ui/LICENSE",
    "imgui": ROOT / "recomp-ui/src/third_party/imgui/LICENSE.txt",
}.items():
    shutil.copy2(source, notices / (name + ".txt"))
sdl = os.environ.get("SNESRECOMP_SDL_BACKEND", "SDL3")
sdl_license = next((p for p in (
    Path(f"/usr/local/share/licenses/{sdl}/LICENSE.txt"),
    Path(f"/usr/share/licenses/{sdl}/LICENSE.txt"),
    Path(f"/usr/share/doc/lib{sdl.lower()}-0/copyright"),
    Path(f"/usr/share/doc/lib{sdl.lower()}-0t64/copyright"),
    Path(f"/usr/share/doc/lib{sdl.lower()}-2.0-0/copyright"),
) if p.is_file()), None)
if sdl_license is None:
    raise SystemExit(f"{sdl} license not installed; stage its notice before packaging")
shutil.copy2(sdl_license, notices / (sdl + ".txt"))

# Preserve the copyright/license records embedded in every shipped font.
for font in (args.appdir / "usr/bin/assets/fonts").glob("*.ttf"):
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
