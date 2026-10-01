"""Pure postprocessing: engine completion and wrapper validation are distinct."""
def summarize(rows, stderr, returncode, framebuffer_sha256):
    finals = [row for row in rows if row.get("kind") == "final"]
    complete = "visual replay complete" in stderr
    no_force = "[fzero-ffb] active" not in stderr
    result = {"engineComplete": complete, "returncode": returncode,
              "ffbInitializationObserved": not no_force, "validated": False}
    errors = []
    if len(finals) != 1:
        errors.append("Expected exactly one final diagnostic record")
    else:
        final = finals[0]
        result.update(simulationFrames=final["simulation_total"],
                      presentations=final["presentations_total"],
                      missed=sum(row.get("missed_delta", 0) for row in rows),
                      targetHz=final["target_hz"], displayHz=final["display_hz"])
        if result["simulationFrames"] != 11364:
            errors.append("Verified frame count differs")
    if returncode != 0 or not complete:
        errors.append("Engine did not complete successfully")
    if not no_force or "physical FFB disabled" not in stderr:
        errors.append("No-force replay proof missing")
    result["framebufferSha256"] = framebuffer_sha256
    if framebuffer_sha256 != "89ac294d9be103c4112672f48fa16648f46501518eb4172ea5d22fbd494ad414":
        errors.append("Final framebuffer differs or is absent")
    result["errors"] = errors
    result["validated"] = not errors
    return result
