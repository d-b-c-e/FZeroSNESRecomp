"""Aggregate interval timings, not final-interval-only timings or GPU costs."""
import argparse
import json
from pathlib import Path

def read(folder):
    result = json.loads((folder / "result.json").read_text())
    if not result["validated"]:
        raise ValueError("Unvalidated replay cannot be compared")
    logs = list((folder / "diagnostics").glob("performance-*.jsonl"))
    if len(logs) != 1:
        raise ValueError("Expected one diagnostic log")
    rows = [json.loads(line) for line in logs[0].read_text().splitlines()]
    stages = {}
    for row in rows:
        for name, timing in row.get("stages", {}).items():
            entry = stages.setdefault(name, {"calls": 0, "total_ms": 0, "max_ms": 0})
            entry["calls"] += timing["calls"]
            entry["total_ms"] += timing["total_ms"]
            entry["max_ms"] = max(entry["max_ms"], timing["max_ms"])
    for stage in stages.values():
        stage["mean_ms"] = stage["total_ms"] / stage["calls"] if stage["calls"] else 0
        stage["ms_per_presentation"] = stage["total_ms"] / result["presentations"]
    return {"result": result, "session": rows[0], "stages": stages}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("instrumented", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    baseline, measured = read(args.baseline), read(args.instrumented)
    for key in ("recordingSha256", "initialStateSha256", "caseSha256", "framebufferSha256"):
        if baseline["result"][key] != measured["result"][key]:
            raise ValueError("Replay identity differs: " + key)
    names = ("left_context", "right_context", "left_upload", "right_upload",
             "left_draw_submit", "right_draw_submit", "left_swap", "right_swap", "center_restore")
    ranked = sorted(({"stage": name, **measured["stages"][name]} for name in names),
                    key=lambda entry: entry["total_ms"], reverse=True)
    evidence = {"baseline": baseline, "instrumented": measured, "rankedSideStages": ranked,
                "qualification": "Main-thread wall time; driver blocking included, no GPU fences. One sequential pair cannot isolate instrumentation overhead or establish an optimization gain."}
    args.output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps({"baseline": baseline["result"], "instrumented": measured["result"], "rankedSideStages": ranked}, indent=2))
