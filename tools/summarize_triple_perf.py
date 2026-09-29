"""Summarize opt-in triple-screen performance JSONL without changing files.

Usage: py -3 tools/summarize_triple_perf.py diagnostics/performance-*.jsonl
Scene is sampled at interval end, so intervals crossing race entry/exit are
approximate. Old draw_submit includes side projection/upload; newer builds
report those as separate stages. Do not compare the old draw mean directly
with a split draw mean.
"""

import argparse
import glob
import json
import sys
from pathlib import Path


STAGES = ("simulation", "ppu", "composition", "upload",
          "triple_projection", "triple_upload", "draw_submit", "present")


class NoSamplesError(ValueError):
    pass


def summarize(rows, scene=2):
    samples = [row for row in rows if row.get("kind") == "sample"
               and row.get("scene") == scene and row.get("simulation_delta", 0)]
    if not samples:
        raise NoSamplesError(f"no scene {scene} samples")
    stage_totals = {name: [0.0, 0] for name in STAGES}
    for sample in samples:
        for name in STAGES:
            stage = sample.get("stages", {}).get(name, {})
            stage_totals[name][0] += float(stage.get("total_ms", 0))
            stage_totals[name][1] += int(stage.get("calls", 0))
    stage_means = {name: total / calls for name, (total, calls)
                   in stage_totals.items() if calls}
    split = stage_totals["triple_projection"][1] > 0
    worst = sorted(samples, key=lambda row: row.get("missed_delta", 0),
                   reverse=True)[:3]
    return {
        "samples": len(samples),
        "missed": sum(row.get("missed_delta", 0) for row in samples),
        "presentations": sum(row.get("presentations_delta", 0) for row in samples),
        "simulation": sum(row.get("simulation_delta", 0) for row in samples),
        "target_hz": samples[-1].get("target_hz"),
        "split": split,
        "stage_means": stage_means,
        "worst": [(row.get("elapsed_ms"), row.get("missed_delta", 0),
                   row.get("frame_interval_p99_ms")) for row in worst],
    }


def read_jsonl(path):
    with Path(path).open(encoding="utf-8") as stream:
        for number, line in enumerate(stream, 1):
            if line.strip():
                try:
                    yield json.loads(line)
                except json.JSONDecodeError as error:
                    raise ValueError(f"{path}:{number}: {error}") from error


def expand_logs(patterns):
    """Expand patterns ourselves: PowerShell may pass wildcards literally."""
    paths = []
    for pattern in patterns:
        matches = sorted(glob.glob(str(pattern)))
        if not matches:
            raise ValueError(f"no logs match {pattern}")
        paths.extend(Path(match) for match in matches)
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--scene", type=int, default=2,
                        help="scene byte to summarize (default: race=2)")
    args = parser.parse_args()
    modes = set()
    try:
        paths = expand_logs(args.logs)
    except ValueError as error:
        parser.error(str(error))
    for path in paths:
        try:
            result = summarize(read_jsonl(path), args.scene)
        except NoSamplesError as error:
            print(f"{path}: skipped ({error})", file=sys.stderr)
            continue
        except (OSError, ValueError) as error:
            parser.error(str(error))
        mode = "split" if result["split"] else "combined"
        modes.add(mode)
        print(f"{path}: {mode}; samples={result['samples']} "
              f"simulation={result['simulation']} "
              f"presentations={result['presentations']} "
              f"missed={result['missed']} target_hz={result['target_hz']}")
        print("  stage mean ms/call: " + ", ".join(
            f"{name}={mean:.3f}" for name, mean in
            result["stage_means"].items()))
        print("  largest missed intervals (elapsed_ms, missed, p99_ms): " +
              "; ".join(str(item) for item in result["worst"]))
    if len(modes) > 1:
        print("Caution: combined draw_submit includes side projection/upload; "
              "split draw_submit does not.")
    if not modes:
        parser.error(f"no scene {args.scene} samples in the matched logs")


if __name__ == "__main__":
    main()
