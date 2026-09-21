"""Private-ROM MSU integration check using synthetic audio, never music assets.

Usage: python tests/test_msu_integration.py --patch-zip PATH --build build-release-162
Requires the owner's stock ROM and locally downloaded Conn/Cubear v11 patch.
Outputs live under ignored captures/msu-validation.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--patch-zip", type=Path, required=True)
    parser.add_argument("--build", default="build-release-162")
    parser.add_argument("--output", type=Path, default=Path("captures/msu-validation"),
                        help="Isolated output directory when testing platforms concurrently")
    args = parser.parse_args()
    out = ROOT / args.output
    out.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.patch_zip) as archive:
        patch = archive.read("f-zero_msu1.ips")
    assert hashlib.sha256(patch).hexdigest() == "9019013f085ff16f5501c4516531a044bc5f36703aadb58844e67c5456413532"
    pack = out / "synthetic pack"
    pack.mkdir(exist_ok=True)
    (pack / "f-zero_msu1.ips").write_bytes(patch)
    # Every track has a distinct DC signature; this checks real playback,
    # track changes and looping without any copyrighted music.
    for track in range(1, 32):
        (pack / f"test-{track}.pcm").write_bytes(
            b"MSU1" + struct.pack("<I", 0) + struct.pack("<hh", track * 100, -track * 100) * 4410)
    rom = ROOT / "fzero.sfc"
    original = hashlib.sha256(rom.read_bytes()).digest()
    env = {k: v for k, v in os.environ.items() if not k.startswith(("FZERO_", "SNESRECOMP_"))}
    env["SNESRECOMP_MSU1"] = f'"{pack}"'  # exercise batch-file quotes + spaces
    exe = ROOT / args.build / ("FZeroSNESRecompHeadless.exe" if os.name == "nt" else "FZeroSNESRecompHeadless")
    for name, contents, diagnostic in (
        ("missing patch", None, "Put Conn/Cubear v11"),
        ("invalid patch", b"PATCHinvalidEOF", "Unsupported MSU-1 patch"),
        ("wrong digest", bytes([patch[0] ^ 1]) + patch[1:], "Unsupported MSU-1 patch"),
    ):
        invalid = out / name
        invalid.mkdir(exist_ok=True)
        if contents is not None:
            (invalid / "f-zero_msu1.ips").write_bytes(contents)
        result = subprocess.run([str(exe), str(rom), "1"], cwd=invalid,
                                env=dict(env, SNESRECOMP_MSU1=str(invalid)),
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 2 and diagnostic in result.stderr, (name, result.stderr)
        assert "Conn/Cubear v11 active" not in result.stderr
        print(f"{name}: refused before cartridge initialization", flush=True)

    # A verified patch but no PCM files must leave the game's SPC fallback
    # functioning, not produce a silent race or hang while waiting for MSU.
    empty_pack = out / "no audio pack"
    empty_pack.mkdir(exist_ok=True)
    (empty_pack / "f-zero_msu1.ips").write_bytes(patch)
    for deluxe in (False, True):
        run = out / ("deluxe" if deluxe else "stock")
        run.mkdir(exist_ok=True)
        run_env = dict(env, SNESRECOMP_SAVE_ROOT=str(run / "saves"),
                       SNESRECOMP_WAV=str(run / "audio.wav"),
                       SNESRECOMP_WRAM_DUMP=str(run / "final.wram"),
                       FZERO_ASPECT="21:9",
                       SNESRECOMP_INPUT_SCRIPT="320-326:8,440-446:8,560-566:8,660-665:32,680-685:32,700-705:32,730-736:8,790-796:8,1160-1899:1,1540-1580:64,1660-1700:64")
        if deluxe:
            run_env["FZERO_DELUXE_DATA"] = str(ROOT / "captures/bs-deluxe/mods/bs-deluxe.dat")
        with (run / "run.log").open("w") as log:
            result = subprocess.run([str(exe), str(rom), "1900"], cwd=run,
                                    env=run_env, stdout=log, stderr=log, timeout=180)
        text = (run / "run.log").read_text()
        print(text[-1800:], flush=True)
        assert result.returncode == 0, run
        assert "Conn/Cubear v11 active" in text
        with wave.open(str(run / "audio.wav"), "rb") as wav:
            audio = wav.readframes(wav.getnframes())
        pairs = list(struct.iter_unpack("<hh", audio))
        # The title track is 4 in both layouts. Most of its playback has no
        # sound effects, so its stereo signature should survive the actual mix.
        matches = sum(left == 400 and right == -400 for left, right in pairs)
        assert matches > 1000, (run, "title PCM not present in mixer", matches)
        ram = (run / "final.wram").read_bytes()
        assert ram[0x183] in range(1, 32) and ram[0x183] != 4, (run, "no music transition", ram[0x180:0x185].hex())
        assert ram[0x182] == 0, (run, "unexpected SPC fallback")
        print(f"{run.name}: {matches} exact synthetic title samples; final track={ram[0x183]}, GP={ram[0x90]}, course={ram[0x53]}", flush=True)

        fallback_env = dict(run_env, SNESRECOMP_MSU1=str(empty_pack / "test"),
                            SNESRECOMP_WRAM_DUMP=str(run / "fallback.wram"),
                            SNESRECOMP_WAV=str(run / "fallback.wav"))
        with (run / "fallback.log").open("w") as log:
            result = subprocess.run([str(exe), str(rom), "1900"], cwd=run,
                                    env=fallback_env, stdout=log, stderr=log, timeout=180)
        assert result.returncode == 0, (run, "missing-audio fallback failed")
        fallback = (run / "fallback.wram").read_bytes()
        assert fallback[0x182] == 1, (run, "SPC fallback flag missing", fallback[0x180:0x185].hex())
        with wave.open(str(run / "fallback.wav"), "rb") as wav:
            fallback_audio = wav.readframes(wav.getnframes())
        assert any(fallback_audio), (run, "silent SPC fallback")
        print(f"{run.name}: missing tracks fall back to non-silent SPC audio", flush=True)
    assert hashlib.sha256(rom.read_bytes()).digest() == original


if __name__ == "__main__":
    main()
