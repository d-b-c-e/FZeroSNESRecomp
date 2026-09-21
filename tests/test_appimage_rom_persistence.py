"""Exercise actual AppImage Browse/zenity selection and launcher restart.

Run under xvfb-run on Linux; requires zenity and xdotool. Takes the owner's
ROM as input and keeps isolated installs/evidence under captures/. No cache
is seeded and no ROM is staged beside the AppImage.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appimage", type=Path, required=True)
    parser.add_argument("--rom", type=Path, default=ROOT / "fzero.sfc")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expect-broken", action="store_true")
    parser.add_argument("--exit-mode", choices=("play", "quit"))
    args = parser.parse_args()
    source, output = args.appimage.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    library = output / "External ROM Library"
    library.mkdir()
    rom = library / "F-Zero (USA).sfc"
    shutil.copy2(args.rom, rom)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_", "APPIMAGE", "APPDIR"))}
    env.pop("WAYLAND_DISPLAY", None)
    env.update(SDL_VIDEODRIVER="x11", GDK_BACKEND="x11", QT_QPA_PLATFORM="xcb", SDL_AUDIODRIVER="dummy",
               SNESRECOMP_AUTOCLOSE_FRAMES="120", SNESRECOMP_SAVE_ROOT="saves")
    results = []

    def xdo(*cmd, **kwargs):
        return subprocess.run(["xdotool", *map(str, cmd)], check=True,
                              capture_output=True, text=True, timeout=25, **kwargs).stdout.strip()

    def run(app, name, pick=False, play=True):
        stage = app.parent
        script = "size:1100x880;wait:120;"
        if pick:
            script += f"shot:{name}-before.png;click:220,555;wait:480;"
        script += f"shot:{name}.png;"
        script += "click:970,820;wait:120;quit" if play else "quit"
        # The extraction runtime can reuse its temporary tree. Give each
        # fresh install a separate one, including any executable-side caches.
        tmpdir = stage / "runtime-tmp"
        tmpdir.mkdir(exist_ok=True)
        with (stage / f"{name}.log").open("w") as log:
            proc = subprocess.Popen([str(app), "--appimage-extract-and-run"],
                                    cwd="/tmp", env=dict(env, LNG_SCRIPT=script, TMPDIR=str(tmpdir),
                                                        GSETTINGS_BACKEND="memory",
                                                        XDG_CONFIG_HOME=str(stage / "desktop-config"),
                                                        XDG_DATA_HOME=str(stage / "desktop-data")),
                                    stdout=log, stderr=log)
            try:
                if pick:
                    dialog = xdo("search", "--sync", "--onlyvisible", "--class", "zenity").splitlines()[-1]
                    xdo("windowfocus", "--sync", dialog)
                    # Mapping the top-level X window precedes GTK's entry
                    # focus/layout; wait for that before typing a long path.
                    time.sleep(.5)
                    xdo("key", "--window", dialog, "--clearmodifiers", "ctrl+l")
                    time.sleep(.3)
                    xdo("key", "--window", dialog, "--clearmodifiers", "ctrl+a")
                    xdo("type", "--window", dialog, "--clearmodifiers", "--delay", "1", str(rom))
                    time.sleep(.2)
                    geometry = dict(line.split("=", 1) for line in xdo("getwindowgeometry", "--shell", dialog).splitlines())
                    # Resolve the typed location, then confirm the highlighted
                    # file using the mouse rather than a second Enter.
                    xdo("key", "--window", dialog, "--clearmodifiers", "Return")
                    time.sleep(1)
                    visible = subprocess.run(["xdotool", "search", "--onlyvisible", "--class", "zenity"],
                                             capture_output=True, text=True, timeout=5)
                    if dialog in visible.stdout.splitlines():
                        xdo("mousemove", "--window", dialog,
                            int(geometry["WIDTH"]) - 40, int(geometry["HEIGHT"]) - 30)
                        xdo("click", "1")
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        visible = subprocess.run(["xdotool", "search", "--onlyvisible", "--class", "zenity"],
                                                 capture_output=True, text=True, timeout=5)
                        if dialog not in visible.stdout.splitlines():
                            break
                        time.sleep(.1)
                    else:
                        subprocess.run(["import", "-window", "root", str(stage / "picker-timeout.png")], timeout=10)
                        raise AssertionError("native zenity picker did not accept the selected ROM")
                code = proc.wait(timeout=60)
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
        text = (stage / f"{name}.log").read_text()
        assert code == 0, (code, text[-3000:])
        assert (stage / f"{name}.png").is_file()
        played = "simulation=120" in text
        cache = stage / "rom.cfg"
        cached = cache.read_text().strip() if cache.exists() else None
        results.append(dict(case=stage.parent.name, step=name, played=played, cache=cached))
        print(f"{stage.parent.name}/{name}: played={played}, cache={cached!r}", flush=True)
        return played, cached

    for exit_mode in ((args.exit_mode,) if args.exit_mode else ("play", "quit")):
        stage = output / exit_mode / "Install With Spaces"
        stage.mkdir(parents=True)
        app = stage / source.name
        shutil.copy2(source, app)
        app.chmod(0o755)
        assert not (stage / "rom.cfg").exists()
        played, cached = run(app, "select", pick=True, play=exit_mode == "play")
        assert played == (exit_mode == "play")
        if args.expect_broken:
            assert cached is None
            assert not run(app, "restart")[0]
        else:
            assert cached and Path(cached).samefile(rom)
            for attempt in range(2):
                played, cached = run(app, f"restart-{attempt + 1}")
                assert played and Path(cached).samefile(rom)
    (output / "results.json").write_text(json.dumps(dict(
        appimage_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        expected_broken=args.expect_broken, results=results), indent=2) + "\n")
    print(f"{'REPRODUCED' if args.expect_broken else 'PASS'}: {output}", flush=True)


if __name__ == "__main__":
    main()
