# Adaptive renderer development

Work is tracked by central Beads feature `beads-wrca`, beneath `beads-8wg.5`.
The implementation branch is `feat/fzero-adaptive-renderer`. This document
describes the renderer contracts; completion and validation remain in Beads.

The current change is a **ROM-independent foundation**, not a playable
widescreen implementation. The desktop and headless hosts still use their
original rendering and frame loop. No game-state addresses or spawn hooks
have been guessed, and the new library is not yet connected to the hosts.

## Agreed behavior

- Authentic 4:3 uses the stock PPU renderer and stock gameplay visibility.
- An opt-in custom Mode 7 compositor owns enhanced race presentation at
  16:9, 21:9, 32:9, or fit-to-window, clamped to 4:3–32:9. Menus retain a
  centered 4:3 picture. HUD groups anchor to the outer edges.
- Actual gameplay activation/spawning follows the viewport. Changes apply at
  simulation boundaries and must be replayable; resizing can change gameplay.
- Simulation stays at 60.098811862 Hz. Auto presentation follows display
  refresh, capped at 360 Hz, with 60 Hz fallback. Fixed choices are 60, 90,
  120, 144, 165, 240, and 360. Interpolation must use validated scene/camera
  and object identities, with resets after discontinuities.
- Windows SDL3 is the first validation target. The owner's playtest is the
  final checkpoint before closure, merge, or release.

## Foundation interfaces

`fzero_video.h` owns settings, physical output geometry, HUD anchor arithmetic,
and independent simulation/presentation deadlines. Simulation debt is retained;
only overdue presentations are skipped. The future host must pump events between
bounded catch-up batches and explicitly reset its clock after pause/minimize/load.
The interpolation alpha addresses previous-to-current completed snapshots,
introducing one simulation interval of presentation latency rather than predicting
unavailable game state.

Internal widths preserve the stock 256-pixel picture's 7:6 display pixel aspect:
342, 448, and 682 pixels for the fixed wide presets. Widths are even so the
stock center is exact; destination rectangles retain the requested physical
aspect. The 684-pixel capacity is separate from the shared PPU's smaller buffers.
Never pass the native output width to the stock PPU buffers.

Settings serialize to a dedicated caller-selected file (planned host filename:
`fzero-video.ini`). Save uses an atomic replacement; malformed recognized values
report failure and retain safe per-field defaults.

`fzero_mode7.h` implements a renderer-owned per-scanline affine transform, signed
sampling outside the stock viewport, hardware map overflow behavior, periodic
origin interpolation, and world-to-scanline projection. The register decoding
follows the pinned framework's `PpuDrawBackground_mode7` semantics. These
functions do not read or write guest globals. Sampling returns palette indices;
layer ordering, colour math, fades, mosaic policy, HUD ownership, and frame
snapshot publication still belong to the forthcoming compositor integration.

Interpolation must only be called after the publisher proves continuity. The
primitive cannot identify scene transitions, camera cuts, object reuse, or
whether two scanlines belong to the same world view. Projection requires
unwrapped world coordinates in the same coordinate system as the captured line.

## ROM-independent validation

From PowerShell, with the working directory at this worktree root:

```powershell
& 'C:/msys64/mingw64/bin/cmake.exe' -S . -B build-tests -G Ninja `
  -DFZERO_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
  -DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe
& 'C:/msys64/mingw64/bin/cmake.exe' --build build-tests
& 'C:/msys64/mingw64/bin/ctest.exe' --test-dir build-tests --output-on-failure
```

The video suite checks ten simulated minutes at eight display rates, missed
deadlines without lost simulation debt, pause/load resets, fractional refresh,
aspect clamps, destination bars, HUD anchors, and settings replacement/validation.
The Mode 7 suite checks both wide margins, flips, rotation, map wrapping and
overflow fill, inverse projection, periodic interpolation, and invalid transforms.
Checks stay active in Release builds. These synthetic tests do not establish
retail-game rendering, spawning, timing, audio, or full-course correctness.

## Integration prerequisite

The starting checkout has no `fzero.sfc` or `src/gen`. Stage the verified USA
v1.0 ROM (SHA-256 from the main README) locally and regenerate before resuming
the stock baseline and object/camera investigation. Keep ROM-derived output
untracked. Reproduce `beads-8wg.5.2` separately from enhanced changes.

After state mapping, connect immutable scanline and world snapshots to the
native compositor, implement verified viewport-following activation hooks,
wire the launcher and SDL host, then execute the agreed course/aspect/FPS,
save/load, resize, complete-cup, and 30-minute soak matrix. A synthetic-test
pass is not the final owner-validation handoff.
