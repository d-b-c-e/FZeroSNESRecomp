# Changelog

## 1.5.0 - 2026-09-18

- BS Deluxe is now compiled into the executable, so every download carries it
  and a missing or damaged `mods/bs-deluxe.dat` can no longer stop the game
  from starting. The embedded bytes are verified exactly as a file was: magic,
  declared sizes, the stock digest they were built against, ordered
  non-overlapping records, and the digest of the patched cartridge. A file
  beside the executable, or `FZERO_DELUXE_DATA`, is still tried first as a
  development override; if anything fails, the game logs and runs stock for
  that session without rewriting the settings file. Release packaging now
  refuses to build without the embedded payload.
- Corrected the streamed-square test to retail's 16-unit block grid. The
  anchor's low bits do not move the square, so 138,481 of 7,323,648 measured
  cells were classified outside it and up to 0.106% of 32:9 margin samples kept
  a stale tile; both are zero now.
- Every mod now ships on by default and the aspect defaults to Fit, which
  follows the window between 4:3 and 32:9. A first run with no
  `fzero-video.ini` therefore starts with Widescreen on at Fit, Presentation
  FPS on at Auto, and BS Deluxe on. `FzeroVideoStock()` is the stock baseline
  and is what the headless host, `FZeroRenderCapture` and the runtime's own
  pre-host viewport use, so captures and tests are unchanged unless
  `FZERO_ASPECT` opts in. A build configured without the BS Deluxe native
  module logs that it is starting stock instead of refusing to launch.

- Fixed the widescreen Mode 7 draw distance, reported by PowerPanda: pieces of
  the track were missing in the margins and appeared only once they reached the
  middle of the screen. Retail streams the tilemap for the stock 256-pixel
  viewport - `$03:9243` keeps one 1024-by-1024-unit world square uploaded, which
  fills the tilemap exactly - so a widened viewport sampled outside it and read
  the tiles another part of the course had left behind. The compositor now
  resolves those samples through retail's own course tables in WRAM bank `$7F`,
  which the frame snapshot already carries. Samples inside the streamed square
  still read the live tilemap, stock 4:3 output is byte-identical, and no guest
  state is written. Non-player cars were measured and are unaffected: their only
  horizontal visibility test is already widened, and what removes them is
  retail's longitudinal window and proximity cull, identical at every aspect.
- Fixed occasional full-screen flicker introduced by that change on the
  interpolated presentation path. Retail writes either representative of the
  camera's map position - some frames the camera's own value, some that plus
  1024 - and an interpolated scanline origin takes the shortest path across
  that seam, so subtracting the current frame's representative moved every
  Mode 7 sample a whole map period for one presentation. The centre and the
  camera are now blended the same periodic way as the origin. Measured over
  321 consecutive race frames at 21:9: seven presentations changed more than
  10,000 pixels against 1.4.3, the worst 63,091 of 100,352 (63% of the frame);
  after the fix none do, and the worst is 946. Output at full blend is
  unchanged.
- Added `tools/measure_draw_distance.py` and a `--sequence[=alpha]` mode for
  `FZeroRenderCapture` that replays a whole race through the compositor in
  order, including the interpolated presentation path.

## 1.4.3 - 2026-09-18

- Updated the bundled BS F-Zero Deluxe mod from upstream USA v1.0 to v1.1
  (upstream April 1, 2025). v1.1 adds the recovered BS F-Zero Grand Prix 2
  Week 1 data (Forest course graphics, Forest I/II track and path, BS-1 League
  race parameters, adjusted Forest III), fixes an upstream course-load CPU
  crash and garbled records graphics, and makes opponent speeds and Exploding
  Bumper spawn rates league-aware.
- Regenerated the namespaced Deluxe native module and the guarded cartridge
  delta from the v1.1 image; the runtime now verifies the v1.1 digest and
  rejects v1.0 data. `patches/bs-deluxe-usa.ips` is now the upstream v1.1 USA
  patch.
- The Deluxe import tools accept both the v1.0 and v1.1 archive layouts and
  read the upstream version from the archive readme.

Validated on the regenerated 1.4.3 build: all five test suites pass, plus the
patch-tool unit tests for both archive layouts. A scripted power-on route
reaches a BS-1 League Forest I Grand Prix race with Deluxe enabled and runs
1,800 frames in the desktop host at 21:9 (144 Hz presentation, SDL dummy
drivers) and in the headless host at 16:9; captured frames show the eight-
machine grid, the BS-1/BS-2 league list, the Forest I course card and the
race. A 600-frame stock boot without Deluxe stays on the stock cartridge and
writes no Deluxe save directory. The v1.1 module keeps the interpreter-floor
scheduler policy from 1.3.0. Full-cup coverage remains a user playtest item.

## 1.4.2 - 2026-09-08

- Extended live title and start-sequence scenery to the selected aspect ratio
  while keeping logos, menu text, and course-selection artwork together.
- Kept the race HUD adaptive from its first setup frame through attract-demo
  exit fades; intro lives counters now retain their right-edge position.
- Fixed the split Training course map and the centered timer after a Training
  crash. Training and Grand Prix loss screens now use their own HUD layouts.
- Fixed an oversized red/gray panel on the Grand Prix loss screen caused by
  extending its collapsed color window into the widescreen margins.

Accepted in interactive playtesting. All five regression suites pass, with
focused coverage for scene transitions, temporary sprite reservations, and
loss-screen color windows. Captured Training, loss, and attract-exit frames
preserve stock-width output.

## 1.4.1 - 2026-09-08

- Fixed distant scenery disappearing or changing abruptly in widescreen,
  especially at 21:9. Both skyline layers now sample the full panorama across
  section boundaries, including the partially filled final section.
- Added 15 clean interpreter fallback observations to the static coverage
  profile and regenerated with the native analyzer. The generated program now
  contains 468 AOT-eligible variants, up from 410 in the previous build.
- Preserved stock-width rendering, HUD placement, sprite visibility safeguards,
  and the interpreter policy used for widescreen opponent projection.

The skyline fix was accepted in an interactive owner playtest. Regression tests
cover both background panoramas across all aspect modes; 104 captured
frame/aspect comparisons preserve the original center and lower track/HUD.
All five test suites pass after regeneration. A 10,800-frame 21:9 stock soak
matches the previous build byte-for-byte in final WRAM and framebuffer, and a
1,800-frame BS Deluxe desktop smoke run completes successfully.

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

## 1.3.0 â€” 2026-09-07

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

## 1.1.0 â€” 2026-09-07

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
