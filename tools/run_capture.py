"""Run a bounded, isolated headless route and capture native renderer inputs."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("name")
p.add_argument("--frames", type=int, default=3600)
p.add_argument("--checkpoints", default="600,1200,1800,2400,3000,3600")
p.add_argument("--inputs", default="")
p.add_argument("--aspect", default="4:3")
p.add_argument("--build", default="build-dev")
p.add_argument("--viewport-script", default="")
p.add_argument("--desktop-fps", type=int, default=None)
p.add_argument("--lifecycle", action="store_true")
a = p.parse_args()
folder = ROOT / "captures" / a.name
folder.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for k in list(env):
    if k.startswith(("FZERO_", "SNESRECOMP_")):
        del env[k]
if os.name == "nt":
    env["PATH"] = "C:/msys64/mingw64/bin;" + env.get("PATH", "")
env.update(FZERO_CAPTURE_FRAMES=a.checkpoints,
           FZERO_CAPTURE_PREFIX=str(folder / "frame"),
           SNESRECOMP_INPUT_SCRIPT=a.inputs,
           SNESRECOMP_SAVE_ROOT=str(folder / "saves"),
           SNESRECOMP_FRAME_DUMP=str(folder / "final.ppm"),
           SNESRECOMP_WRAM_DUMP=str(folder / "final.wram"),
           FZERO_ASPECT=a.aspect)
env["FZERO_VIEWPORT_SCRIPT"] = a.viewport_script
if a.lifecycle:
    env["FZERO_LIFECYCLE_TEST"] = "1"
host = "FZeroSNESRecompHeadless"
if a.desktop_fps is not None:
    host = "FZeroSNESRecomp"
    env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
               SNESRECOMP_AUTOCLOSE_FRAMES=str(a.frames),
               FZERO_VIDEO_CONFIG=str(folder / "fzero-video.ini"))
    (folder / "fzero-video.ini").write_text(
        f"EnhancedRenderer={int(a.aspect != '4:3')}\nAspect={a.aspect}\nPresentationEnabled=1\nPresentationFPS={a.desktop_fps}\n")
with (folder / "run.log").open("w") as log:
    result = subprocess.run([str(ROOT / a.build / (host + (".exe" if os.name == "nt" else ""))),
                             str(ROOT / "fzero.sfc"), str(a.frames)],
                            cwd=folder, env=env, stdout=log, stderr=log)
print((folder / "run.log").read_text()[-1500:])
raise SystemExit(result.returncode)
