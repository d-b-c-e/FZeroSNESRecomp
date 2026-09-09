# Adaptive renderer — 1.1.0

Implemented in the isolated `feat/fzero-adaptive-renderer` worktree, tracked
by central Beads `beads-wrca` beneath the F-Zero epic `beads-8wg.5`. The owner
accepted the visual checkpoint and authorized integration/release.

## Independent plugins

| Plugin | Settings | Disabled behavior |
|---|---|---|
| Widescreen (`fzero-widescreen`) | 16:9, 21:9, 32:9, Fit | Stock 4:3 |
| Presentation FPS (`fzero-presentation-fps`) | Auto, 60, 90, 120, 144, 165, 240, 360 | Original cadence |

`fzero-video.ini` beside the executable persists `EnhancedRenderer`, `Aspect`,
`PresentationEnabled`, and `PresentationFPS`. Initial combined-checkpoint
settings migrate automatically. `FZERO_VIDEO_CONFIG` overrides the path for
isolated validation. Ctrl+F6 cycles aspect; Ctrl+F7 enables/cycles FPS.

## Rendering and gameplay

`fzero_renderer.c` owns immutable double-buffered frames containing per-line
PPU registers, palette and OAM, plus VRAM, stock pixels and published game RAM.
It samples Mode 7 beyond the stock viewport, composites Mode 1 backgrounds and
sprites, and preserves colour math, windows and fades. It never writes guest
state. The shared PPU's smaller wide buffers remain disabled. Internal widths
are 342, 448 and 682 pixels at 224 lines; scaling preserves stock pixel aspect.
Fit clamps to 4:3–32:9.

The skyline uses overlapping 512-by-56-pixel strips, rather than a single
wrapping background map. Retail `$A60C` selects a strip through vertical scroll
while keeping horizontal scroll in 0–255. BG1 spans 896 panorama pixels with
scroll bases 36/92/148/204; BG2 spans 768 with bases 92/148/204. Padding beyond
the stock-visible overlap caused scenery to disappear or change abruptly in
the wide margins. The compositor now maps margin pixels into the full panorama
and samples the corresponding strip's first 256 columns. This applies only to
the recognized skyline layout and leaves the stock center and guest state
unchanged.

The `fix/widescreen-background-culling` regression checks both panoramas across
all aspect modes and rotation boundaries, including partially filled final
strips. All five CTest suites pass, and the new panorama test fails against the
old renderer. Comparison of 104 saved-frame/aspect pairs (stock races, an
attract route and BS Deluxe Forest) preserved every center and lower-track/HUD
pixel; 50 outputs corrected skyline margins. Before/after inspection at 21:9
confirmed that the abrupt skyline cutoff in the captured turn is gone.
Both Windows hosts built successfully. A bounded SDL dummy-driver desktop run
completed 2,200 simulation frames at the 144 FPS presentation setting (5,270
presentations, zero missed); this is an automated smoke check, not a human
playtest or display-performance measurement.

Race BG3 and HUD reservations anchor to the outer edges. The power meter's
composed fill follows its outline. Title screens extend their live track and
skyline to the selected viewport; flat selection/loading screens extend the
PPU backdrop with its brightness and colour-window effects. Original title/menu
artwork and course-intro text remain centered. Hidden player/effect reservations stay hidden even when their
stale tile data lies within the expanded viewport.

HUD anchoring begins when race setup has installed its graphics (`$55=2`,
`$56!=0`), not only when active racing begins (`$55>=3`). Retail `$8ACD`
installs the HUD and `$8B11` advances the setup substate. Waiting for active
racing left the already-visible HUD at 4:3 positions during setup (24 frames
in the captured stock attract transition, longer during a GP start). BG3,
the sprite reservations, and the power-meter fill share this readiness flag.
The earlier intro/setup text remains centered. Its spare-machine icon/count
uses temporary OBJ slots 126/127, written by `$B164` at `$03F8/$03FC`, before
moving to race slots 22/23. Those temporary counters use the same right anchor
from their first visible frame. No previous-frame layout is
latched, so reset and direct snapshot loads use the correct layout immediately.

The `fix/hud-transition-layout` regression passes all five CTest suites and
fails against the old renderer. Across 440 captured frame/aspect comparisons
covering stock attract, GP start and BS Deluxe attract, 105 wide setup outputs
correct their HUD positions; stock-width, menu, intro and active-race outputs
remain identical.

Vehicle identity comes from DMA ordering pointers `$0AC0..$0ACA`, which select
six 32-byte reservations. Used-tile counts at `$11D0` suppress unused opponent
reservation tails. Interpolation follows identity across OAM sorting changes,
rejects changed attributes and large motion, and resets on discontinuities and
loads. Camera interpolation handles periodic coordinates and rejects scene jumps.

GP loss phase `$55=6,$58=0` reuses the temporary counter slots and centers its message.
Keep the score left anchored and the counter right anchored, but do not apply
the race power-meter copy or extend the collapsed colour window. Otherwise the
one-column red/gray HDMA residue at the stock left edge becomes a wide panel.
The original one-column edge residue remains; stock-width output is preserved.
Training (`$58!=0`) instead retains the live race HUD on loss, so its timer
keeps the right anchor. Training's course-selector map also reuses slots
126/127; those are map pieces, not the GP spare-machine counter, and stay with
the other centered map pieces. Attract exit `$54=3,$55=4/5` retains the race HUD
through its fade instead of briefly centering it when the scene state changes.

The opponent projection routine `$00:DBC4` runs through the interpreter so a
pre-opcode policy at `$00:DCC6` can extend its horizontal interval `[-32,288)`
by the current viewport's extra columns. Original callers own activation flags,
allocation-related distance metrics, graphics selection and disappearance.
Depth/longitudinal limits and pool size retain original behavior. This changes
actual game state, not just drawing; aspect changes can affect gameplay. No ROM
patch or generated-C edit is used.

`fzero_video.c` schedules the original 60.098811862 Hz simulation independently
from presentation. Bounded catch-up batches retain simulation debt; only overdue
presentations are skipped. Pause, minimize and load explicitly reset pacing.
Auto follows display refresh with a 60 Hz fallback and 360 Hz cap. Native
interpolation uses previous/current completed snapshots, adding one simulation
interval of latency. Stock 4:3 repeats authentic frames.

## Validation evidence

- Four Release CTest suites pass: video/config/clock/replay, Mode 7, renderer
  bounds/identity/layout, and independent plugin toggles/options/persistence.
- Eleven captured menu, intro and race frames matched stock pixels exactly
  when the native compositor was evaluated at stock width.
- An 1,800-frame GP input route produced identical RAM at 60/240 desktop FPS
  and in the wide headless run.
- A replay through 32:9, 4:3, 21:9 and Fit at two window sizes matched headless
  and desktop at 60/144 FPS. Final RAM SHA-256:
  `dacd0de1393c0ad9a264de5c1fc9891b15ebc8f9e26de441670cdb94167233a6`.
- Snapshot save/load reproduced ten subsequent frames in RAM and master clock;
  soft reset completed a 3,600-frame lifecycle route.
- Wide attract soak, 108,180 frames: `resume=00803c`, `master=38660019778`,
  `logic_changes=108110`, `video_active=107381`, `video_changes=79913`,
  `audio_samples=57767662`, `audio_active=108017`, `audio_peak=17644`,
  `audio_underruns=4`. This is 30 simulated minutes, not a desktop wall-clock
  benchmark, and preceded the final HUD/plugin fixes.
- Owner feedback identified stray hidden sprites and separated title characters.
  Focused regression tests and an inspected intro capture verified the fixes;
  the owner accepted the updated checkpoint.

Coverage is not exhaustive across all courses or complete cups. The owner
preferred a fast human checkpoint over further broad automated checks. Linux,
macOS and SDL2 were not validated for this release. Three pre-existing
scene-transition raster/HDMA discrepancies remain in `beads-8wg.5.2`. An earlier
optional Python-analysis build failed in attract mode; release generation uses
the supported native backend. Selected FPS targets are not performance guarantees.

## Reproduction and packaging

Run `ctest --test-dir <build> --output-on-failure`. Pure video/replay and Mode 7
tests also build with `-DFZERO_BUILD_GAME=OFF`, without a ROM. The private
`tools/run_capture.py` input grammar is `FIRST[-LAST]:MASK`; viewport events use
`FRAME:ASPECT[@WIDTHxHEIGHT]`. `--desktop-fps` uses SDL dummy drivers and
`--lifecycle` exercises disk snapshots and reset. `FZeroRenderCapture` renders
local source captures and compares stock-width output against stock pixels.
Raw captures are compiler-dependent diagnostics, not portable save states.

`VERSION` owns the release number. Regenerate without
`SNESRECOMP_EMIT_AOT_DENY_GATE`, build Release, and run `tools/make_release.py`.
Packages contain launcher assets, runtime DLLs and notices; they exclude ROMs,
generated C, personal settings, saves and captures.
