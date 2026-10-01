"""Pure postprocessing: engine completion and wrapper validation are distinct."""
import hashlib
import json
import math

def summarize(rows, stderr, returncode, framebuffer_sha256, processing_error=None):
    complete = "visual replay complete" in stderr
    no_force = "[fzero-ffb] active" not in stderr
    result = {"engineComplete": complete, "returncode": returncode,
              "ffbInitializationObserved": not no_force, "validated": False}
    errors = [processing_error] if processing_error else []
    try:
        if any(not isinstance(row, dict) or not isinstance(row.get("kind"), str) for row in rows):
            raise ValueError("Diagnostic rows must be objects with a kind")
        sessions = [row for row in rows if row["kind"] == "session"]
        finals = [row for row in rows if row["kind"] == "final"]
        if len(sessions) != 1 or sessions[0].get("schema") != 1:
            raise ValueError("Expected exactly one schema-1 session")
        if len(finals) != 1:
            raise ValueError("Expected exactly one final diagnostic record")
        for row in rows:
            for key in ("simulation_total", "presentations_total", "simulation_delta", "presentations_delta", "missed_delta"):
                if key in row and (type(row[key]) is not int or row[key] < 0):
                    raise ValueError("Invalid nonnegative integer counter: " + key)
            for stage in row.get("stages", {}).values():
                if type(stage["calls"]) is not int or stage["calls"] < 0:
                    raise ValueError("Invalid stage calls")
                for key in ("total_ms", "mean_ms", "max_ms"):
                    value = stage[key]
                    if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
                        raise ValueError("Invalid stage timing: " + key)
        final = finals[0]
        for key in ("simulation_total", "presentations_total", "target_hz", "display_hz"):
            value = final[key]
            if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
                raise ValueError("Invalid required final field: " + key)
        result.update(simulationFrames=final["simulation_total"], presentations=final["presentations_total"],
                      missed=sum(row.get("missed_delta", 0) for row in rows),
                      targetHz=final["target_hz"], displayHz=final["display_hz"])
        if result["simulationFrames"] != 11364:
            errors.append("Verified frame count differs")
    except (KeyError, TypeError, ValueError, AttributeError) as error:
        errors.append("Diagnostic postprocessing failed: " + str(error))
    if returncode != 0 or not complete:
        errors.append("Engine did not complete successfully")
    if not no_force or "physical FFB disabled" not in stderr:
        errors.append("No-force replay proof missing")
    result["framebufferSha256"] = framebuffer_sha256
    result["framebufferScope"] = "center source framebuffer before CRT/display presentation"
    result["sidePixelsVerified"] = False
    if framebuffer_sha256 != "89ac294d9be103c4112672f48fa16648f46501518eb4172ea5d22fbd494ad414":
        errors.append("Final framebuffer differs or is absent")
    result["errors"] = errors
    result["validated"] = not errors
    return result

def postprocess(folder, stderr, returncode):
    rows, framebuffer_sha256 = [], None
    processing_error = None
    try:
        logs = list((folder / "diagnostics").glob("performance-*.jsonl"))
        if len(logs) != 1:
            raise ValueError("Expected exactly one diagnostic log")
        rows = [json.loads(line) for line in logs[0].read_text().splitlines()]
        frame = folder / "final-source-frame.bmp"
        if frame.exists():
            framebuffer_sha256 = hashlib.sha256(frame.read_bytes()).hexdigest()
    except Exception as error:
        processing_error = "Postprocessing failed: " + type(error).__name__ + ": " + str(error)
    return summarize(rows, stderr, returncode, framebuffer_sha256, processing_error)

def persist(path, result):
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, indent=2) + "\n")
    temporary.replace(path)
