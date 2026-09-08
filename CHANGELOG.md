# Changelog

## 1.4.0 - 2026-09-08

- Added launcher Display shader support for F-Zero, including staged presets:
  CRT Soft, LCD Grid, Sharp, and Warm Composite.
- Added an OpenGL GLSL presentation path used when a shader is selected. The
  no-shader path keeps the existing SDL renderer behavior.
- Added Display aspect choices for F-Zero: 4:3, 16:9, 21:9, 32:9, and Fit to
  window. The built-in Widescreen/Presentation mod remains authoritative over
  aspect when enabled.
- Added widescreen and BS Deluxe screenshots to the README.
- Added a 21:9 BS Deluxe race screenshot to the README.
- Added the BS Deluxe USA IPS patch to the repo and release ZIP at
  `patches/bs-deluxe-usa.ips`.
- Added Linux x86_64 AppImage release packaging.
- Updated BS Satellaview README credit/permission wording.

Validated with direct unit test executables, no-shader and CRT shader desktop
smoke runs, and a live CRT window capture after fixing the OpenGL VAO binding
needed by the shared GLSL renderer.

## 1.3.0 — 2026-09-07

Private feature release, advancing two minor versions from 1.1.0 as requested.

- Added BS F-Zero Deluxe USA 1.0 as one independent, all-or-nothing mod:
  original and BS courses, eight machines, alternate leagues/layouts, records
  and Practice ghosts. The stock ROM file remains unchanged.
- Retained separate Widescreen and Presentation FPS plugins, adaptive HUD
  anchoring, wider opponent projection and interpolation independent of logic.
- Added a separately namespaced Deluxe module and isolated 32 KiB SRAM saves.
  Deluxe currently runs its main scheduler through the interpreter floor;
  native interrupt helpers and the custom renderer remain active.
- Fixed the car-selection HDMA bus-read crash and the shared PPU's missing
  XOR/AND/XNOR window operations that hid league and difficulty text.
- Added original North American SNES box art to the launcher.

Validated both Blue Thunder on Forest I and Blue Falcon on Mute City I in
visible desktop runs. Five game test suites and the focused framework dispatch
and PPU regressions pass. Full-cup coverage and ghost recording/playback remain
unverified. See [BS Deluxe details](docs/BS_DELUXE_EXPLORATION.md).

The private release can include the locally verified Deluxe delta and upstream
credits. The packaging tool refuses this payload unless the F-Zero repository
is private. No stock/patched ROM or user save is included.

## 1.1.0 — 2026-09-07

- Added a native Mode 7 renderer for 16:9, 21:9, 32:9 and Fit to window.
- Preserved stock 4:3 rendering. Race HUD groups anchor to the outer edges;
  menus and course-intro text remain centered.
- Extended opponent projection/activation to the current viewport through
  original game routines. Viewport changes apply at simulation boundaries.
- Added separate Widescreen and Presentation FPS built-in plugins in recomp-ui,
  each with independent toggles and settings.
- Added Auto refresh and 60/90/120/144/165/240/360 FPS presentation. Simulation
  stays at 60.098811862 Hz; native camera/car interpolation never writes game state.
- Added aspect/FPS shortcuts, pause/minimize pacing, soft reset, deterministic
  input/window replays and snapshot capture tools.
- Fixed stale sprites appearing in wide margins and separated course-title text.

Owner playtest accepted the checkpoint for release. See
[validation evidence and limits](docs/ADAPTIVE_RENDERER.md).
This is the first semantic-version tag; the previous WIP binary was labeled 1.0.
