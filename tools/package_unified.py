"""Package an explicitly receipted stock-only Windows candidate; never run it.

Build receipt JSON:
 {"schema":"dbce.fzero-build", "version":1, "productVersion":"1.8.3",
  "sourceRevision":"<40 hex>", "sourceTree":"<40 hex>", "dirty":false,
  "bsDeluxe":false, "dependencies":{"snesrecomp":"<40 hex>",
  "recomp-ui":"<40 hex>"}, "files":{"FZeroSNESRecomp.exe":"<sha256>",
  "WheelFfb.dll":"<sha256>", "licenses/wheel-toolkit.txt":"<sha256>", ...}}
The receipt must be generated at build time from the exact approved staging
inputs; this tool validates consistency, not compiler provenance.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DOCS = ("SETUP.md", "UNIFIED-PRODUCT.md", "PLAYTHROUGH_RECORDING.md",
        "TRIPLE_SCREEN_FEASIBILITY.md", "TELEMETRY_SIGNALS.md",
        "ADAPTIVE_RENDERER.md", "BS_DELUXE_EXPLORATION.md", "HD_MODE7.md",
        "HD_MODE7_PERFORMANCE.md", "PERFORMANCE_DIAGNOSTICS.md", "SAVE_STATES.md",
        "WHEELFFB_EXPERIMENTAL_RECEIPT.md")
SOURCE_FILES = ("README.md", "LICENSE", "VERSION", "game-product.json",
                "lib/toolkit/MANIFEST.txt", "lib/toolkit/VERSION", "lib/toolkit/LICENSE.txt",
                "tools/fzero_replay_adapter.py")
SOURCE_FILES += tuple("docs/screenshots/" + n for n in (
    "attract-race.png", "bs-blue-thunder.png", "bs-forest-iii-race-21x9.png",
    "bs-forest-iii-race.png", "bs-forest-iii.png", "widescreen-title.png"))
UI_ASSETS = tuple("assets/img/" + n for n in (
    "boxart.tga", "brand_mark.tga", "flags.png", "pad.tga", "verdict_bad.tga",
    "verdict_none.tga", "verdict_ok.tga", "verdict_warn.tga")) + tuple(
    "assets/fonts/" + n for n in ("LatoLatin-Bold.ttf", "LatoLatin-Regular.ttf",
    "NotoSansSymbols2-Regular.ttf", "OpenMoji-black-glyf.ttf"))
SETUP = b'@echo off\r\ncd /d "%~dp0"\r\n"%~dp0FZeroSNESRecomp.exe" --launcher\r\n'
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll",
              "shell32.dll", "ole32.dll", "oleaut32.dll", "comdlg32.dll",
              "comctl32.dll", "ws2_32.dll", "winmm.dll", "imm32.dll",
              "version.dll", "setupapi.dll", "hid.dll", "dinput8.dll",
              "dxguid.dll", "opengl32.dll", "dwmapi.dll", "uxtheme.dll",
              "ntdll.dll", "msvcrt.dll", "ucrtbase.dll", "shlwapi.dll",
              "cfgmgr32.dll", "rpcrt4.dll", "bcrypt.dll", "secur32.dll"}
SYSTEM_DLLS |= {"d2d1.dll", "dwrite.dll"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def relative(name):
    path = PurePosixPath(name)
    if not name or "\\" in name or ":" in name or path.is_absolute() or any(
            p in ("", ".", "..") for p in name.split("/")):
        raise ValueError(f"Unsafe payload path: {name}")
    return path


def approved(name):
    path = relative(name)
    low = name.lower()
    if low in ("fzerosnesrecomp.exe", "fzerosnesrecompheadless.exe") or (len(path.parts) == 1 and low.endswith(".dll")):
        return True
    if path.parts[0] == "licenses" and path.suffix.lower() in (".txt", ".md"):
        return True
    if name in UI_ASSETS:
        return True
    if low.startswith("assets/shaders/") and path.suffix.lower() in (".glsl", ".glslp") and "crt-geom" not in low:
        return True
    raise ValueError(f"Unapproved payload (private/user data forbidden): {name}")


def pe_imports(data):
    """Read only normal PE imports, rejecting malformed/non-x64 images."""
    try:
        if data[:2] != b"MZ":
            raise ValueError("Not a PE image")
        offset = struct.unpack_from("<I", data, 60)[0]
        if data[offset:offset + 4] != b"PE\0\0":
            raise ValueError("Invalid PE signature")
        machine, sections = struct.unpack_from("<HH", data, offset + 4)
        opt_size = struct.unpack_from("<H", data, offset + 20)[0]
        opt = offset + 24
        if machine != 0x8664 or struct.unpack_from("<H", data, opt)[0] != 0x20b:
            raise ValueError("Expected Windows x64 PE32+")
        section_table = opt + opt_size

        def rva(value):
            for i in range(sections):
                pos = section_table + i * 40
                size, va, raw_size, raw = struct.unpack_from("<IIII", data, pos + 8)
                if va <= value < va + max(size, raw_size):
                    result = raw + value - va
                    if result >= len(data):
                        break
                    return result
            raise ValueError("PE import RVA outside sections")

        if struct.unpack_from("<I", data, opt + 108)[0] < 14:
            raise ValueError("Incomplete PE data directories")
        # Delay imports need a separate audited resolver; do not silently omit.
        if struct.unpack_from("<I", data, opt + 112 + 13 * 8)[0]:
            raise ValueError("Delay imports require explicit packaging support")
        table = struct.unpack_from("<I", data, opt + 120)[0]
        if not table:
            return set()
        pos, names = rva(table), set()
        for _ in range(4096):
            entry = struct.unpack_from("<IIIII", data, pos)
            if not any(entry):
                return names
            start = rva(entry[3])
            end = data.find(b"\0", start, start + 256)
            if end < 0:
                raise ValueError("Unterminated PE import")
            name = data[start:end].decode("ascii").lower()
            if "/" in name or "\\" in name or not name.endswith(".dll"):
                raise ValueError("Unsafe PE import name")
            names.add(name)
            pos += 20
        raise ValueError("Unterminated PE import table")
    except (struct.error, UnicodeError) as error:
        raise ValueError("Malformed PE import table") from error


def read_payload(payload, receipt, product_version, toolkit_hash):
    if receipt.get("schema") != "dbce.fzero-build" or receipt.get("version") != 1:
        raise ValueError("Unsupported build receipt")
    if receipt.get("dirty") is not False or receipt.get("bsDeluxe") is not False:
        raise ValueError("Requires clean stock-only build; private BS module excluded")
    if receipt.get("productVersion") != product_version:
        raise ValueError("Build/product version mismatch")
    for key in ("sourceRevision", "sourceTree"):
        if not re.fullmatch(r"[0-9a-f]{40}", receipt.get(key, "")):
            raise ValueError(f"Invalid {key}")
    deps = receipt.get("dependencies", {})
    if set(deps) != {"snesrecomp", "recomp-ui"} or any(
            not re.fullmatch(r"[0-9a-f]{40}", v) for v in deps.values()):
        raise ValueError("Exact submodule identities required")
    files, result, seen = receipt.get("files", {}), {}, set()
    for name, expected in files.items():
        approved(name)
        if name.lower() in seen:
            raise ValueError("Case-colliding payload")
        seen.add(name.lower())
        path = payload.joinpath(*relative(name).parts)
        if any(p.is_symlink() for p in (path, *path.parents)) or not path.resolve().is_relative_to(payload.resolve()):
            raise ValueError("Symlink/path escape in payload")
        data = path.read_bytes()
        if not isinstance(expected, str) or digest(data) != expected.lower():
            raise ValueError(f"Payload hash mismatch: {name}")
        if b"BSDELX1" in data:
            raise ValueError("Embedded private BS payload excluded")
        result[name] = data
    required = {"FZeroSNESRecomp.exe", "FZeroSNESRecompHeadless.exe", "WheelFfb.dll", "assets/img/boxart.tga",
                "licenses/snesrecomp.txt", "licenses/recomp-ui.txt", "licenses/imgui.txt",
                "licenses/wheel-toolkit.txt"} | set(UI_ASSETS)
    required |= {"licenses/" + Path(n).stem + ".txt" for n in UI_ASSETS if n.endswith(".ttf")}
    if not required.issubset(result):
        raise ValueError(f"Missing required payload: {sorted(required - result.keys())}")
    if digest(result["WheelFfb.dll"]) != toolkit_hash.lower():
        raise ValueError("WheelFfb override needs explicit source repin")
    binaries = {name.lower(): data for name, data in result.items() if name.lower().endswith((".exe", ".dll"))}
    for name, data in binaries.items():
        for imported in pe_imports(data):
            if imported not in binaries and imported not in SYSTEM_DLLS and not imported.startswith(("api-ms-", "ext-ms-")):
                raise ValueError(f"Unresolved runtime DLL: {name} -> {imported}")
    return result


def zip_bytes(files):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    return stream.getvalue()


def verify(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(set(n.lower() for n in names)) != len(names):
            raise ValueError("Duplicate ZIP entries")
        for name in names:
            relative(name)
        manifest = json.loads(archive.read("manifest.json"))
        if manifest.get("schema") != "dbce.fzero-package" or manifest.get("schemaVersion") != 1:
            raise ValueError("Unsupported package manifest")
        expected = manifest["files"]
        if set(names) != set(expected) | {"manifest.json"}:
            raise ValueError("Package file set differs from manifest")
        for name, hash_value in expected.items():
            if digest(archive.read(name)) != hash_value:
                raise ValueError(f"Package hash mismatch: {name}")
        receipt = manifest["build"]
        payload_names = receipt["files"]
        extras = set(SOURCE_FILES) | {"Setup.cmd"} | {"docs/" + n for n in DOCS}
        if set(expected) != set(payload_names) | extras:
            raise ValueError("Unapproved package file set")
        if manifest.get("productId") != "fzero-snes-recomp" or manifest.get("channel") != "candidate" or manifest.get("distribution") != "stock-only-rom-free":
            raise ValueError("Wrong product/distribution identity")
        product = json.loads(archive.read("game-product.json"))
        if product.get("id") != manifest["productId"] or archive.read("VERSION").decode().strip() != manifest["version"]:
            raise ValueError("Product version/identity differs from manifest")
        pin = archive.read("lib/toolkit/MANIFEST.txt").decode()
        match = re.search(r"(?mi)^([0-9a-f]{64})[ \t]+native/WheelFfb\.dll[ \t]*\r?$", pin)
        if not match or match.group(1).lower() != manifest["wheelRuntimeSha256"]:
            raise ValueError("Package runtime differs from retained toolkit pin")
        for name, hash_value in payload_names.items():
            if expected.get(name) != hash_value.lower():
                raise ValueError("Package/build receipt hashes differ")
        # Revalidate the receipt boundaries and binary identities without extracting.
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name in payload_names:
                destination = root.joinpath(*relative(name).parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(archive.read(name))
            read_payload(root, receipt, manifest["version"], manifest["wheelRuntimeSha256"])
    return manifest


def package(payload, receipt, output, root=ROOT):
    product = json.loads((root / "game-product.json").read_text())
    version = (root / "VERSION").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Invalid VERSION")
    pin = (root / "lib/toolkit/MANIFEST.txt").read_text()
    match = re.search(r"(?mi)^([0-9a-f]{64})[ \t]+native/WheelFfb\.dll[ \t]*\r?$", pin)
    if not match:
        raise ValueError("Missing native runtime toolkit pin")
    wheel_hash = match.group(1).lower()
    files = read_payload(payload, receipt, version, wheel_hash)
    git = lambda *args: subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()
    if git("rev-parse", receipt["sourceRevision"] + "^{tree}") != receipt["sourceTree"]:
        raise ValueError("Build receipt tree does not match source revision")
    if git("rev-parse", "HEAD") != receipt["sourceRevision"] or git("status", "--porcelain", "--untracked-files=no"):
        raise ValueError("Package from the exact clean build revision")
    links = dict((line.split()[3], line.split()[1]) for line in git("ls-files", "--stage", "snesrecomp", "recomp-ui").splitlines())
    if links != receipt["dependencies"]:
        raise ValueError("Build dependency pins differ from source gitlinks")
    # Only repository shader files are accepted, never user-imported presets.
    shaders = {n: (root / n).read_bytes() for n in git("ls-files", "assets/shaders").splitlines()}
    for name, data in files.items():
        if name.startswith("assets/shaders/") and shaders.get(name) != data:
            raise ValueError("Unreviewed or altered shader in payload")
    if not set(shaders).issubset(files):
        raise ValueError("Bundled shader set incomplete")
    for name in SOURCE_FILES:
        files[name] = (root / name).read_bytes()
    for name in DOCS:
        files["docs/" + name] = (root / "docs" / name).read_bytes()
    files["Setup.cmd"] = SETUP
    manifest = {"schema": "dbce.fzero-package", "schemaVersion": 1,
                "productId": product["id"], "version": version, "channel": "candidate",
                "distribution": "stock-only-rom-free", "build": receipt,
                "wheelRuntimeSha256": wheel_hash,
                "files": {name: digest(data) for name, data in sorted(files.items())}}
    files["manifest.json"] = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    data = zip_bytes(files)
    verify(io.BytesIO(data))
    output.mkdir(parents=True, exist_ok=False)
    archive = output / f"FZeroSNESRecomp-{version}-unified-candidate-windows-x64.zip"
    archive.write_bytes(data)
    archive.with_suffix(".zip.sha256").write_text(f"{digest(data)}  {archive.name}\n", encoding="ascii")
    verify(archive)
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--payload", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify", type=Path)
    args = parser.parse_args()
    try:
        if args.verify:
            print(json.dumps(verify(args.verify), indent=2))
        elif args.payload and args.receipt and args.output:
            print(package(args.payload, json.loads(args.receipt.read_text()), args.output))
        else:
            parser.error("Specify --verify or --payload, --receipt and --output")
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
