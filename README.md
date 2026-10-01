# FZeroSNESRecomp

A native PC build of *F-Zero* for SNES.

You bring your own legally dumped *F-Zero (USA)* ROM. No ROM is included.

<p align="center">
  <img src="docs/screenshots/bs-forest-iii-race-21x9.png" width="96%" alt="BS F-Zero Deluxe Forest III race in 21:9">
  <br>
  <img src="docs/screenshots/widescreen-title.png" width="48%" alt="F-Zero title screen in widescreen">
  <img src="docs/screenshots/bs-blue-thunder.png" width="48%" alt="BS F-Zero Deluxe Blue Thunder machine select">
  <br>
  <img src="docs/screenshots/bs-forest-iii-race.png" width="48%" alt="BS F-Zero Deluxe Forest III race in 16:9">
  <img src="docs/screenshots/bs-forest-iii.png" width="48%" alt="BS F-Zero Deluxe Forest III course select">
</p>

## Features

- Play *F-Zero* as a native app.
- Widescreen modes: 16:9, 21:9, 32:9, and Fit.
- Optional [HD Mode 7](docs/HD_MODE7.md): 2x through 10x track rendering, independent of widescreen and presentation FPS.
- High refresh presentation: 60, 90, 120, 144, 165, 240, or 360 FPS.
- Display shaders: CRT Soft, LCD Grid, Sharp, Warm Composite, or your own GLSL shader.
- Experimental configurable triple-screen projection on equal-panel fullscreen Surround/span displays or three equal, horizontally aligned separate displays (three borderless SDL windows). Both layouts support independent per-panel CRT shaders. Side vehicles and effects remain incomplete; separate displays still need physical-rig validation.
- Save states with a slot browser and thumbnails, opened with **F7** or **Select + R**.
- Rewind: step back through the last few seconds and drop back in.
- Gamepad support through SDL.
- Optional proportional wheel steering on the SNES digital input.
- Optional BS F-Zero Deluxe content.
- Optional MSU-1 music packs for stock F-Zero and BS Deluxe (bring your own patch and audio).

## Download And Play

On Windows:

1. Download the Windows x64 ZIP from the Releases page.
2. Extract the whole ZIP.
3. Run `FZeroSNESRecomp.exe`.
4. Pick your own *F-Zero (USA)* `.sfc` or `.smc` ROM when asked.

On Linux:

1. Download the Linux AppImage from the Releases page.
2. Put your own *F-Zero (USA)* `.sfc` or `.smc` ROM next to it.
3. Make the AppImage executable.
4. Run it.

The app remembers your ROM path after the first launch.

Enable **Skip launcher on boot** to start directly with that ROM next time.
The choice is saved as `[General] SkipLauncher=1` in `config.ini` beside the
executable. Run `FZeroSNESRecomp.exe --launcher` (on Linux, run your AppImage
with `--launcher`) or set the value to `0` to return to the launcher. The override also works
with a ROM path before or after it. A missing or invalid remembered ROM opens
the launcher so you can select a valid copy.

## Settings

Open **Settings > Display** for:

- **Aspect ratio:** 4:3, 16:9, 21:9, 32:9, or Fit.
- **Shader:** None (default), CRT Soft, LCD Grid, Sharp, Warm Composite, or a custom shader.

Open **Mods** for:

- **Widescreen:** makes races wider.
- **Presentation FPS:** makes motion smoother on high refresh screens.
- **HD Mode 7:** sharper tracks at integer scales from 2x to 10x; off by default.
- **Diagnostics:** optional local performance reports for troubleshooting; off by default. See [how to capture a report](docs/PERFORMANCE_DIAGNOSTICS.md).
  Start at 2x. Above 4x can cause severe slowdown; use at your own risk.
- **BS Deluxe:** adds the Satellaview machines, leagues, and tracks.

When the Widescreen mod is on, its aspect setting wins over the normal Display aspect setting.

### Importing CRT-Geom or another shader

In **Settings > Display**, use **Browse** beside Shader to select your own
`.glslp` preset or `.glsl` shader. Keep the preset's accompanying files and
subdirectories intact: for example, `crt-geom.glslp` needs
`shaders/crt-geom.glsl` beside it. The app remembers the selected path; it
does not copy the pack, so leave it in a permanent location. RetroArch Slang
(`.slangp`) presets are not supported by this OpenGL path.

[CRT-Geom is available upstream](https://github.com/libretro/glsl-shaders/tree/master/crt).
It carries GPL-2.0-or-later terms. It is **not bundled**: redistribution
compatibility with this app's differently licensed dependencies has not been
established. User-selected CRT-Geom has been tested through the existing shader
loader. An unreadable or invalid preset falls back to unfiltered output.

### MSU-1 music

1. Obtain the **Conn/Cubear v11** patch for your mode: the
   [stock F-Zero patch](https://www.zeldix.net/t1447p200-f-zero) or the
   [BS Deluxe patch](https://www.zeldix.net/t2768-bs-f-zero-deluxe-msu-1).
2. Put `f-zero_msu1_stock.ips` (stock) or `f-zero_msu1.ips` (BS Deluxe) in
   your music pack's folder alongside its numbered `.pcm` tracks. Both patch
   files may coexist. Keep the pack's original track numbering and common
   filename prefix.
3. Enable **MSU-1** in **Settings > Sound** and select that folder.

Keep using your **unmodified USA ROM**. The app verifies the selected v11 patch
(stock: 784 bytes, SHA-256 `e37b51f11692422c5d09e6f26b003f7bf46d651159b25ee7025089400176fecd`;
Deluxe: 709 bytes, SHA-256 `9019013f085ff16f5501c4516531a044bc5f36703aadb58844e67c5456413532`)
and applies it in memory, **after BS Deluxe** if enabled. No ROM file is
rewritten. The patches and music are not included in downloads. Missing or
unsupported patches produce a warning and leave the original soundtrack active.
The patches handle missing PCM tracks through their original-audio fallback.

Stock and Deluxe have been exercised with synthetic tracks and a user-supplied
JUD6MENT pack, widescreen, and high-refresh presentation. Compatibility is
limited to these two supported cartridge layouts;
arbitrary third-party ROM patches are not accepted or claimed compatible.

MSU sessions currently execute the patched cartridge through the interpreter,
so compiled stock routines cannot bypass its audio hooks. Normal sessions
retain native dispatch. Save states and rewind restore the selected song from
its beginning, not its exact playback position. MSU saves are separate from
non-MSU saves, as shown below.

For command-line use, `SNESRECOMP_MSU1` can select a pack folder or filename
prefix; `off` overrides a saved enabled setting. `FZERO_MSU1_PATCH` can point
to the v11 IPS in another folder.

## Save States And Rewind

### The save-state menu

Press **F7**, or hold **Select + R** on a gamepad, to open the save-state
browser. The game freezes while it is open, so a state you take is that exact
moment.

- **Up / Down** or the **arrow keys** pick one of 12 slots.
- **A** on a pad, or **X** on the keyboard, loads the selected slot.
- **X** on a pad, or **S** on the keyboard, saves to it.
- **B** on a pad, or **Escape**, closes the menu without doing anything.
- **1**-**9** jump straight to a slot.

Each slot shows a thumbnail of the moment it was saved, so you can tell them
apart without loading them.

### Rewind

Press **F8**, or hold **Select + L** on a gamepad, to open the rewind
filmstrip. It shows the recent past as a strip of frames:

- **Left / Right** scrub back and forward. Hold a direction to keep scrubbing.
- **A** on a pad, or **Enter** / **Space**, jumps to the selected moment.
- **B** on a pad, or **Escape**, leaves without changing anything.

Rewind is **off by default**, because it keeps whole snapshots of the machine
in memory. Turn it on in the launcher under **Settings**, where you can also
set:

- **Rewind depth:** how many snapshots to keep (50, 100, 150, or 200).
- **Rewind interval:** how many frames apart they are (1, 4, 8, 12, 15, or 30).

More snapshots make the history longer; a shorter interval makes it finer.
Both cost memory: one snapshot of *F-Zero* is about 330 KB, so 100 of them is
roughly 33 MB. At the default 50 snapshots every 15 frames you can step back
about 12 seconds.

### Changing the keys

Both keys are rebindable in the launcher's **Controls** page, as
**SaveStateMenu** and **Rewind**. They are saved to `config.ini` next to the
executable and take effect the next time you start the game. The rewind
switch, depth and interval are remembered in the same file.

### Racing-wheel setup in the launcher

The launcher opens before Play when started with `--launcher`. On its **Mods**
page, the **Controls** group contains **Racing wheel controls** and **Force
feedback**. Click an axis or button binding, then move or press that control
on the wheel; optional binds can be cleared with Backspace, and Escape cancels
capture. Accelerator and brake inversion are checkboxes. The FFB device is a
dropdown of currently connected force-feedback devices. Button binds include the four
directions, every SNES face/shoulder/system button, **Open save-state menu**,
and **Open rewind**. These are independent of the keyboard bindings on the
launcher's **Controls** page. Turn rewind on under **Settings** before using
its wheel button.

For a first wheel test, try **Center dead zone 0%**, **Travel to full steering
100%**, and **Response curve 50%**. The latter two are the defaults for raw
wheels, making center steering stronger while preserving full travel and
tapering sensitivity near the outside; tune them to taste in the launcher.
The **Steering preview** above those controls updates as you turn the selected
wheel and change these values. It shows the raw axis position and the share of
SNES simulation frames that will hold Left or Right; the bar is a preview of
the digital pulse density, not a true analog steering angle. The existing
**Live wheel input** section below the bindings still shows raw axes/buttons.
The **FFB device name** must uniquely match
the DirectInput wheel reported by the toolkit log. Force feedback remains
opt-in and should be tested with modest strength first.

Under **Settings > Audio**, disable **MSU-1** for original cartridge
music, or enable it and select a supported music pack for enhanced music. Use
**Settings > Display > Shader > None** for an unfiltered image; also turn off linear
filtering there for the sharpest pixels. These settings do not require an
in-game menu.

### SimHub telemetry

The Windows build can emit a Forza Horizon-compatible 324-byte UDP packet for
SimHub and other dashboards. It is off by default. Add this to `config.ini`
beside the executable:

```ini
[Telemetry]
Enabled=1
Host=127.0.0.1
Port=8000
```

Select the Forza Horizon UDP protocol and the same port in the dashboard.
Speed, position, race time, energy, steering, throttle, and brake are derived
from the game's live WRAM. F-Zero has no engine or gearbox, so RPM and gear are
compatibility values; lap and race position are not yet exported. Network
errors only disable telemetry and never stop the game.

Packet serialization comes from dbce-wheel-mod-toolkit v0.13.0, pinned under
`lib/toolkit` so release builds are reproducible.

### Steering-wheel force feedback

Windows builds stage the pinned dbce-wheel-mod-toolkit v0.13.0 runtime beside
the game. Force feedback is off by default and requires one unique DirectInput
device name; the game refuses to fall back to a different wheel. Add this to
`config.ini` beside the executable:

```ini
[ForceFeedback]
Enabled=1
Device=MOZA R12 Base
Strength=40
```

These fields can also be set through the launcher. Use the exact name shown
by Windows or the toolkit log. `Strength` is clamped
to 0-100. The model provides speed-sensitive steering resistance, a hardware
damper, track texture, and finite collision pulses. If the configured wheel or
`WheelFfb.dll` is missing, the game continues with force feedback disabled.
The toolkit's watchdog, exit guards, and panic-stop path prevent stale forces.

The quick-slot keys - **F1**-**F12** to load a slot, **Shift + F1**-**F12**
to save one - still work. Where a binding above uses a key (F7 and F8 by
default), that binding wins and the quick slot behind it is unavailable; both
slots are still reachable from the menu. Rebinding SaveStateMenu or Rewind to
another key hands the F-key straight back.

### Analog wheel steering

The SNES controller has only digital Left and Right buttons, but an SDL
gamepad axis can retain proportional steering by distributing those button
presses over successive simulation frames. Enable it for one controller by
adding `AnalogSteering = 1` to that controller's GUID section in `config.ini`:

```ini
[Controller.030000006e3400000600000000000000]
AnalogSteering = 1
Deadzone = 0
SteeringRangePercent = 100
SteeringResponsePercent = 50
```

Full wheel lock is held every frame. Smaller deflections produce evenly
distributed presses, while returning to centre or changing direction clears
the pending pulse immediately. The setting is opt-in and the default digital
gamepad behavior is unchanged.

True racing wheels that SDL does not classify as gamepads use the same GUID
section. Raw axis and button indices are zero-based; omitted pedals and
buttons stay unbound:

```ini
[Controller.030000006e3400000600000000000000]
AnalogSteering=1
SteeringAxis=0
AcceleratorAxis=2
BrakeAxis=3
PedalThreshold=0
ButtonL=12
ButtonR=13
ButtonSelect=23
ButtonStart=36
```

Set `AcceleratorInvert=1` or `BrakeInvert=1` when an axis runs backwards.
Available button keys are `ButtonA`, `ButtonB`, `ButtonX`, `ButtonY`,
`ButtonL`, `ButtonR`, `ButtonSelect`, `ButtonStart`, `ButtonUp`, `ButtonDown`,
`ButtonLeft`, `ButtonRight`, `ButtonSaveStateMenu`, and `ButtonRewind`.

### Where states are kept

Stock *F-Zero* and BS F-Zero Deluxe keep separate states, because they are
different cartridges and their snapshots are not interchangeable:

| Mode | Slot files |
| --- | --- |
| Stock | `saves/fzero<N>.sav` |
| BS F-Zero Deluxe | `saves/bs-deluxe/fzero-bs-deluxe<N>.sav` |
| Stock + MSU-1 | `saves/msu1/fzero-msu1<N>.sav` |
| BS Deluxe + MSU-1 | `saves/bs-deluxe/msu1/fzero-bs-deluxe-msu1<N>.sav` |

A thumbnail sits beside each as `.sav.thumb`. If a state from the other mode
somehow ends up in a slot, loading it is refused and the game keeps running -
the title bar says so and nothing is disturbed.

## BS F-Zero Deluxe

BS F-Zero Deluxe is included with permission from its authors:
GuyPerfect, Porthor, and PowerPanda.

The release includes two BS Deluxe files:

- `mods/bs-deluxe.dat` is used by this app.
- `patches/bs-deluxe-usa.ips` is the upstream v1.1 USA SNES patch for your own ROM.

No patched ROM is included.

## If The Game Crashes

Send these files from the game folder:

- `crash_report_*.json`
- `crash_minidump_*.dmp`
- `last_run_report.json`

## Build From Source

Clone with submodules:

```bash
git clone --recurse-submodules git@github.com:mstan/FZeroSNESRecomp.git
cd FZeroSNESRecomp
```

Put your own USA ROM at `fzero.sfc`, then generate and build:

```bash
bash tools/regen.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Local ROMs, generated files, saves, captures, and builds are ignored by git.

Run the ROM-free regression suites with `ctest --test-dir build --output-on-failure`.
With your local ROM and the upstream patch ZIP, `tests/test_msu_integration.py`
checks stock/Deluxe playback using generated test tones, missing-track fallback
and invalid-patch rejection. On Windows, `tests/test_desktop_integration.py`
additionally checks real launcher persistence, imported shaders, controller
overlays and save/load. Both scripts accept `--help` and keep their test data
under ignored `captures/` directories; neither bundles music or a shader.

`python tests/test_rom_persistence.py --source build` checks ROM selection
through the actual Windows file dialog, then Play/close and repeated restarts.
It also checks cancellation, an invalid pick, and selecting a moved ROM. It
requires an interactive desktop and never seeds the ROM cache itself.

`python tests/test_launcher_options.py --source build --rom PATH
--output captures/launcher-options` exercises the real Skip Launcher control,
relaunch/recovery, and typed HD resolution persistence. On Linux, prefix it
with `xvfb-run -a`. `--appimage-layout` also checks settings beside an AppImage
instead of inside its executable directory, using the AppImage environment.

On Linux, `xvfb-run -a python3 tests/test_appimage_rom_persistence.py
--appimage PATH --output captures/rom-persistence-linux` exercises the packaged
AppImage through the real zenity picker, then Play/quit and repeated relaunches.
It requires `zenity` and `xdotool`; use a fresh output directory for each run.

## License

MIT License, Copyright (c) 2026 Matthew Stanley. See `LICENSE`. Bundled dependencies keep their own licenses under `licenses/` in each release; BS F-Zero Deluxe content is included with its authors' permission and is not covered by this license.

*F-Zero* belongs to Nintendo. The game ROM is not included.
