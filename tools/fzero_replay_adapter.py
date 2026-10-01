"""Bridge a verified F-Zero drive into the toolkit's device-free replay format.

Requires the toolkit's tools/replay/replay_case.py via --toolkit. The game owns
its .fzpt/state codec and pure FFB model; this adapter owns only identity and
normalization. It never loads WheelFfb.dll or opens a controller.
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


MODEL_PROFILE = b"fzero-ffb@2:spring=present;damper=present;road=present;constant=fallback-only"
MASTER_HZ = 21477272  # SNES NTSC master clock; raw ticks are actual master cycles.


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact(root: Path, path: Path, format_name: str) -> dict:
    resolved = path.resolve(strict=True)
    relative = resolved.relative_to(root.resolve()).as_posix()
    return {"path": relative, "sha256": sha256(resolved), "format": format_name}


def drive_header(path: Path) -> tuple[str, int]:
    with path.open("rb") as stream:
        header = stream.read(49)
        if len(header) != 49 or header[:8] != b"FZPT0001" or header[48] != 1:
            raise ValueError("incomplete or unsupported F-Zero drive")
        count = struct.unpack_from("<Q", header, 40)[0]
    if not 0 < count <= 2_000_000 or path.stat().st_size != 49 + 12 * count:
        raise ValueError("F-Zero drive frame count/length mismatch")
    return header[8:40].hex(), count


def ini(path: Path) -> configparser.ConfigParser:
    config = configparser.ConfigParser(interpolation=None)
    with path.open("r", encoding="utf-8-sig") as stream:
        config.read_file(stream)
    return config


def msu_patch(pack: Path, config: configparser.ConfigParser,
              video: configparser.ConfigParser) -> Path | None:
    if not config.getboolean("Sound", "Msu1Enabled", fallback=False):
        return None
    patch = "f-zero_msu1.ips" if video.getboolean("FZeroVideo", "BSDeluxe", fallback=False) else "f-zero_msu1_stock.ips"
    result = pack / patch
    if not result.is_file():
        raise ValueError(f"MSU patch missing: {result}")
    return result


def create_case(args: argparse.Namespace) -> None:
    root = args.case.resolve().parent
    if args.case.exists():
        raise ValueError("case already exists; original case identity is immutable")
    if args.state.resolve() != Path(str(args.drive.resolve()) + ".state"):
        raise ValueError("F-Zero loads only the .state sibling of its .fzpt source")
    rom_digest, count = drive_header(args.drive)
    config = ini(args.config)
    video = ini(args.video)
    if video.getboolean("FZeroVideo", "BSDeluxe", fallback=False):
        raise ValueError("BS Deluxe replay needs a separately pinned Deluxe-data artifact")
    configured_pack = (root / config.get("Sound", "Msu1Dir")).resolve()
    if config.getboolean("Sound", "Msu1Enabled", fallback=False) and configured_pack != args.msu_pack.resolve():
        raise ValueError("MSU pack differs from recorded configuration")
    patch = msu_patch(args.msu_pack, config, video)
    config_copy = root / "diagnostics" / (args.case.stem + ".config.ini")
    video_copy = root / "diagnostics" / (args.case.stem + ".video.ini")
    for original, snapshot in ((args.config, config_copy), (args.video, video_copy)):
        with snapshot.open("xb") as stream:
            stream.write(original.read_bytes())
    artifacts = {
        "config": artifact(root, config_copy, "fzero-config-ini@1"),
        "video": artifact(root, video_copy, "fzero-video-ini@1"),
        "rom": artifact(root, args.rom, "snes-rom@1"),
    }
    if patch:
        # The preview's music directory may be a junction into LaunchBox.
        # Toolkit case artifacts cannot escape the case root, so pin a tiny
        # immutable IPS copy locally and compare the live pack to it on replay.
        patch_copy = root / "diagnostics" / (args.case.stem + ".msu-patch.ips")
        with patch_copy.open("xb") as stream:
            stream.write(patch.read_bytes())
        artifacts["msuPatch"] = artifact(root, patch_copy, "ips@1")
    capture_sha = sha256(args.capture_exe)
    capture_clean = False
    if args.capture_receipt is not None:
        with args.capture_receipt.open("r", encoding="utf-8") as stream:
            receipt = json.load(stream)
        if (set(receipt) != {"schema", "version", "sourceRevision", "sourceTree",
                             "executableSha256", "dirty"} or
                receipt["schema"] != "fzero.capture-build" or receipt["version"] != 1 or
                receipt["sourceRevision"] != args.source_revision or
                receipt["executableSha256"] != capture_sha or receipt["dirty"] is not False):
            raise ValueError("capture clean receipt does not match source/executable")
        source_tree = subprocess.run(["git", "show", "-s", "--format=%T", args.source_revision],
                                     capture_output=True, text=True)
        if source_tree.returncode != 0 or receipt["sourceTree"] != source_tree.stdout.strip():
            raise ValueError("capture clean receipt source tree mismatch")
        capture_clean = True
    case = {
        "schema": "dbce.wheel.replay-case", "version": 1,
        "caseId": args.case_id, "game": "fzero-snes-recomp",
        "adapter": "fzero-fzpt@1", "capability": "game-input-replay",
        "clock": {"domain": "emulated", "ticksPerSecond": MASTER_HZ},
        "source": artifact(root, args.drive, "fzero-fzpt@1"),
        "initialState": artifact(root, args.state, "fzero-snapshot@1"),
        "artifacts": artifacts,
        "provenance": {
            "sourceRevision": args.source_revision,
            # Without a build-time clean receipt, conservatively label the
            # capture provenance unverified/dirty rather than claim it was clean.
            "dirty": not capture_clean,
            "executableSha256": capture_sha,
        },
    }
    # Preserve a separate, unmodified source recording; the digest of the
    # effective cartridge lives inside .fzpt and is checked by the game.
    print(f"drive frames={count} effectiveCartridgeSha256={rom_digest}")
    with args.case.open("xb") as stream:
        stream.write((json.dumps(case, separators=(",", ":"), sort_keys=True) + "\n").encode("utf-8"))


def load_toolkit(path: Path):
    module_path = path / "tools" / "replay" / "replay_case.py"
    spec = importlib.util.spec_from_file_location("dbce_replay_case", module_path)
    if spec is None or spec.loader is None:
        raise ValueError(f"toolkit replay implementation not found: {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verified_source_and_state(case: dict, root: Path) -> tuple[Path, Path]:
    """Match the toolkit-pinned state to what the headless game actually opens."""
    if (case["game"] != "fzero-snes-recomp" or case["adapter"] != "fzero-fzpt@1" or
            case["capability"] != "game-input-replay" or
            case["source"]["format"] != "fzero-fzpt@1" or
            not isinstance(case["initialState"], dict) or
            case["initialState"]["format"] != "fzero-snapshot@1"):
        raise ValueError("not a compatible F-Zero game-input/state case")
    source = (root / case["source"]["path"]).resolve(strict=True)
    state = (root / case["initialState"]["path"]).resolve(strict=True)
    if state != Path(str(source) + ".state"):
        raise ValueError("manifest state is not the .state sibling loaded by F-Zero")
    return source, state


def raw_rows(path: Path, expected_count: int, strength: int):
    """Validate a complete raw stream before the toolkit sink is created."""
    with path.open("r", encoding="ascii", newline="") as stream:
        if stream.readline() != f"FZFFB1\t{strength}\n":
            raise ValueError("raw model header or strength mismatch")
        previous_tick = -1
        for frame in range(expected_count):
            fields = stream.readline().rstrip("\n").split("\t")
            if len(fields) != 9:
                raise ValueError(f"raw model truncated at frame {frame}")
            try:
                values = tuple(int(value) for value in fields)
            except ValueError as error:
                raise ValueError(f"invalid raw model number at frame {frame}") from error
            index, tick, racing, constant, spring, damper, road, frequency, impact = values
            if index != frame or tick <= previous_tick or racing not in (0, 1) or impact not in (0, 1):
                raise ValueError(f"raw frame order/state invalid at {frame}")
            if any(abs(value) > 10000 for value in (constant, spring, damper, road)) or not 0 <= frequency <= 100000:
                raise ValueError(f"raw force out of range at {frame}")
            if impact and not racing:
                raise ValueError(f"off-race impact at {frame}")
            previous_tick = tick
            yield values
        if stream.readline() != f"complete\t{expected_count}\n" or stream.read(1):
            raise ValueError("raw model footer missing or extra data")


def force(sequence: int, tick: int, frame: int, effect: str, family: str,
          operation: str, magnitude: float, frequency: float = 0,
          duration: int | None = None) -> dict:
    return {
        "kind": "force", "sequence": sequence, "tick": tick, "frame": frame,
        "effect": effect, "family": family, "operation": operation,
        "magnitude": magnitude, "frequencyHz": frequency, "durationMs": duration,
    }


def requests(rows, strength: int, impact_strength: int = 20,
             impact_type: str = "Constant"):
    if not 0 <= impact_strength <= 100 or impact_type not in ("Constant", "Sine"):
        raise ValueError("invalid impact trial")
    sequence = 0
    previous_tick = 0
    previous_frame = 0
    for frame, tick, racing, constant, spring, damper, road, frequency, impact in rows:
        # The F-Zero consumer asks for zero constant force when its spring slot
        # exists; constant_force is a fallback model value, not delivered torque.
        for effect, family, magnitude, hz in (
            ("steering", "constant", 0.0, 0.0),
            ("spring", "spring", spring / 10000, 0.0),
            ("damper", "damper", damper / 10000, 0.0),
            ("road", "sine", road / 10000, max(1000, frequency) / 1000),
        ):
            yield force(sequence, tick, frame, effect, family, "set", magnitude, hz)
            sequence += 1
        if impact:
            # Independent crash tuning, matching the current consumer. This
            # is software intent, not proof of runtime support or delivered torque.
            yield force(sequence, tick, frame, "impact",
                        "constant" if impact_type == "Constant" else "sine", "start",
                        impact_strength / 100,
                        0.0 if impact_type == "Constant" else 32.0,
                        120 if impact_type == "Constant" else 140)
            sequence += 1
        previous_tick, previous_frame = tick, frame
    yield force(sequence, previous_tick, previous_frame, "all", "all", "stop_all", 0.0)


def observe(args: argparse.Namespace) -> None:
    toolkit = load_toolkit(args.toolkit)
    identity = toolkit.validate_case(args.case)
    with args.case.open("rb") as stream:
        case = toolkit.decode(stream.read())
    if case["clock"] != {"domain": "emulated", "ticksPerSecond": MASTER_HZ}:
        raise ValueError("not a compatible F-Zero replay case")
    root = args.case.resolve().parent
    source, _ = verified_source_and_state(case, root)
    _, frame_count = drive_header(source)
    config = ini(root / case["artifacts"]["config"]["path"])
    video = ini(root / case["artifacts"]["video"]["path"])
    if video.getboolean("FZeroVideo", "BSDeluxe", fallback=False):
        raise ValueError("BS Deluxe replay needs a separately pinned Deluxe-data artifact")
    if sha256(args.rom) != case["artifacts"]["rom"]["sha256"]:
        raise ValueError("ROM differs from case")
    if video.getboolean("FZeroVideo", "TripleScreen", fallback=False) and video.get("FZeroVideo", "Aspect") == "Fit":
        aspect = "16:9"  # Triple rig's center viewport, not the Surround canvas.
    else:
        aspect = video.get("FZeroVideo", "Aspect")
    if aspect not in ("16:9", "4:3"):
        raise ValueError(f"unsupported recorded center viewport: {aspect}")
    pack = (root / config.get("Sound", "Msu1Dir")).resolve()
    patch = msu_patch(pack, config, video)
    if patch:
        if "msuPatch" not in case["artifacts"] or sha256(patch) != case["artifacts"]["msuPatch"]["sha256"]:
            raise ValueError("MSU patch differs from case")
    elif "msuPatch" in case["artifacts"]:
        raise ValueError("MSU mode differs from case")
    strength = args.strength if args.strength is not None else config.getint("ForceFeedback", "Strength")
    if not 0 <= strength <= 100:
        raise ValueError("strength out of range")
    impact_strength = getattr(args, "impact_strength", None)
    if impact_strength is None:
        impact_strength = config.getint("ForceFeedback", "ImpactStrength", fallback=20)
    impact_type = getattr(args, "impact_type", None) or config.get(
        "ForceFeedback", "ImpactType", fallback="Constant")
    if not 0 <= impact_strength <= 100 or impact_type not in ("Constant", "Sine"):
        raise ValueError("impact strength/type out of range")
    # The original case identity never changes for tuning trials. The trial
    # configuration digest does, and the runner hash identifies its model code.
    runner_sha = sha256(args.runner)
    profile = MODEL_PROFILE + (":impact=" + impact_type + ":software-intent").encode("ascii")
    trial = json.dumps({"strength": strength, "impactStrength": impact_strength,
                        "impactType": impact_type, "runnerSha256": runner_sha,
                        "profile": profile.decode("ascii")}, sort_keys=True,
                       separators=(",", ":")).encode("ascii")
    header = {
        "kind": "header", "schema": "dbce.wheel.force-observation", "version": 1,
        "caseId": case["caseId"], "caseSha256": identity["caseSha256"],
        "clock": case["clock"], "model": "fzero-ffb@2:" + runner_sha[:16],
        "configSha256": hashlib.sha256(trial).hexdigest(),
        "profileSha256": hashlib.sha256(profile).hexdigest(),
        "output": "observe", "physicalOutput": False,
    }
    with tempfile.TemporaryDirectory(prefix="fzero-ffb-observe-") as temporary:
        raw = Path(temporary) / "model.raw"
        environment = {key: value for key, value in os.environ.items()
                       if not key.upper().startswith(("FZERO_", "SNESRECOMP_"))}
        environment.update({
            "FZERO_REPLAY_PLAYTHROUGH": str(source.resolve()),
            "FZERO_ASPECT": aspect,
            "FZERO_FFB_MODEL_STRENGTH": str(strength),
            "FZERO_FFB_OBSERVATION_RAW": str(raw),
        })
        if patch:
            environment["SNESRECOMP_MSU1"] = str(pack)
        run = subprocess.run([str(args.runner), str(args.rom)], env=environment,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if run.returncode != 0:
            raise ValueError(f"verified headless replay failed ({run.returncode}): {run.stderr[-2000:]}")
        # Pass one validates the complete game/model trace before opening any
        # observation file; pass two emits only validated model requests.
        for _ in raw_rows(raw, frame_count, strength):
            pass
        with toolkit.ObservationSink(args.output, header) as sink:
            for request in requests(raw_rows(raw, frame_count, strength), strength,
                                    impact_strength, impact_type):
                sink.emit(request)
    print(f"verified frames={frame_count} caseSha256={identity['caseSha256']} output={args.output}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create", help="pin immutable source/config artifact hashes")
    for name in ("case", "drive", "state", "config", "video", "rom", "msu-pack", "capture-exe"):
        create.add_argument("--" + name, type=Path, required=True)
    create.add_argument("--case-id", required=True)
    create.add_argument("--source-revision", required=True)
    create.add_argument("--capture-receipt", type=Path,
                        help="build-time clean receipt; without it provenance.dirty=true")
    run = sub.add_parser("observe", help="verified game replay to toolkit force observations")
    for name in ("case", "toolkit", "runner", "rom", "output"):
        run.add_argument("--" + name, type=Path, required=True)
    run.add_argument("--strength", type=int)
    run.add_argument("--impact-strength", type=int,
                     help="independent software crash strength, 0-100; no torque is applied")
    run.add_argument("--impact-type", choices=("Constant", "Sine"),
                     help="requested cue model; hardware support is not inferred")
    args = parser.parse_args()
    try:
        (create_case if args.command == "create" else observe)(args)
    except (ValueError, OSError, KeyError, configparser.Error) as error:
        print(f"fzero replay adapter: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
