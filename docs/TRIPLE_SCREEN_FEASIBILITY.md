# Triple-screen feasibility

This note describes what “triple-screen support” can honestly mean for the
F-Zero native recompilation, and how a future adapter can interoperate with the
DBCE triple-screen layout contract without making the public repository depend
on a private package.

## Finding

F-Zero does not expose a conventional 3D scene or camera matrix. The track is
SNES Mode 7: each scanline contains an affine map from screen X to a texel in a
flat world plane. Vehicles, effects, scenery, and the HUD are SNES tile and OAM
layers composed in screen space.

## Prototype status (2026-09-29)

`src/fzero_triple_geometry.*` now calculates distinct eye rays for three
physical panels, including the bezel gap. `src/fzero_triple_ground.*` is an
experimental flat-ground calibration from two captured Mode 7 scanlines. A
deterministic test checks panel symmetry, independent sightlines, and that
center-panel ground rays reproduce synthetic source scanlines. The separate
integration branch now has an opt-in three-panel ground compositor, but the
normal installed game still uses one wide view. Side-panel sprite reprojection
is not active yet.

The ground adapter also has an inverse projection from an unwrapped Mode 7
ground coordinate to subpixel coordinates on any physical panel. Synthetic
and captured-race calibrations round-trip through all three panel planes in
ROM-free tests. This is geometry groundwork for placing world-anchored objects;
it does not recover missing OAM artwork, infer a vehicle's exact ground anchor,
or make the current side buffers render vehicles or effects.
For the two visible opponents in captured frame 1800, mapping their wrapped
WRAM course positions through the frame's Mode 7 center and inverse camera
lands within 3 native horizontal pixels and 5 vertical pixels of the guest's
recorded screen anchors. The screen-locked player has a separate vertical
offset; treating all six cars' artwork as though its top-left were the ground
contact would be wrong. The side-view visibility/OAM generation problem is
still open.
The read-only `FZeroTripleRuntimeCapture capture.bin --vehicles` probe reports
each car's WRAM world and guest-screen anchors, its scanline-100 non-sentinel
vehicle OAM reservation count, and projected panel coordinates. In 56 private
captures sampled every 200 replay frames, two state-active car anchors landed
on a side panel; one (frame 6200, right panel) had no OAM reservation to move.
This demonstrates a missing-artwork case in the captured frame, but the car's
state flags have not been fully decoded, so it is not proof that that car
should have been visible to the player. Reprojection must be gated by a
verified live/visible game state, not just a plausible world coordinate.
`FZERO_TRIPLE_CAMERA_TRACE=1` runs the same read-only check on every headless
replay frame. The complete 11,364-frame recorded drive passed its state hashes:
9,846 of 9,847 race-state frames accepted the triple calibration, with one
rejection at frame 1042 near race entry. It found 47 projected side-panel
opponent-anchor occurrences; 43 had non-sentinel OAM reservations at scanline
100, including car 3 with eight reserved slots near the right panel's inner edge
at frame 2777. Four had no OAM reservations (car 4 in state
`CC`, at frames 4652 and 6197–6199). The state meaning remains unresolved;
these numbers describe data availability, not intended car visibility or
physical display output. No later sustained-race calibration rejection was
observed in that recording.
The opt-in camera trace logs the first twelve side anchors with reservations,
including guest-screen position and slot count, to select immutable captures
for an offline sprite experiment without changing guest memory. A reservation
alone does **not** prove a complete drawable vehicle. A read-only raster probe
now counts decoded nontransparent OBJ pixels in a 684-pixel diagnostic view
and reports their native logical bounds. All 43 reserved side occurrences in
this recording contained some pixels, but the first car-3 occurrence (headless
frame 2777, capture 2778) contained only ten pixels in logical columns
321–326, rows 47–48. The wide reference image shows no recognizable full
opponent there. That car's world anchor projects into the right side panel
while its guest OAM fragment sits just outside the 16:9 center crop. A full
replay classification found 42 of 43 such decoded-art occurrences entirely
outside that crop. The exception at headless frame 10721 overlaps it by only
one logical column; decoded fragment sizes range from 5 to 1,314 pixels.
This makes an owner-filtered side-art preview plausible, but does not solve
perspective sizing, priority, seam clipping, or the four projected anchors
without OAM. No side vehicle layer is enabled yet.
An **offline-only** experiment now uses `FZeroTripleRuntimeCapture
capture.bin sides.ppm --runtime --vehicle-preview` to decode the captured,
owner-tagged opponent OBJ pixels and project each as a center-facing billboard
at its calibrated world anchor. Six private replay captures produced between
21 and 2,012 written side pixels. In montages, the visible car fragments land
plausibly on the side track, but cars crossing an inner edge are clipped because
the stock center image is unchanged. The probe now reports the projected OBJ
billboard bounds on all three planes. At captured frame 4351, car 4's anchor
lands just beyond the center panel (x=522.64 at 512-pixel panel width), but
its approximate art bounds cross center x=498.69–546.60 and right-side
x=-23.14–31.52. Frame 9645 shows the mirror case: left-side anchor x=495.56,
with bounds crossing left x=459.17–538.28 and center x=-56.32–15.54.
Both are evidence that anchor-only culling loses seam art; they are not proof
that every projected OBJ pixel should survive the game's priority and window
rules. A ROM-free fixture now verifies the overlapping-panel geometry.
The preview also lacks a solved priority,
occlusion, and colour-math treatment and cannot invent art for the four
side-anchor occurrences without OAM. It is deliberately not called by the live
SDL presenter; the game remains in `SCREENSPACE_SPRITES` degraded state.
`FZeroTripleRuntimeCapture capture.bin sides.ppm --runtime` writes the exact
512×288 side-buffer size used by the SDL triple presenter. Without that flag,
the tool retains its 640×360 high-resolution comparison fixture. The two
PPM halves must be split at the selected panel width when assembling an
offline three-panel preview; they are not interchangeable crops.

`FZeroTripleGroundCapture` is an offline renderer built from immutable capture
files. It uses the existing course-table/VRAM lookup and colour pipeline to
write three distinct ground panels into one PPM span. A per-scanline similarity
correction matches the center panel's exact captured Mode 7 step and origin
while retaining distinct physical rays on the side panels. This older offline
ground-only tool deliberately renders sky black and omits all vehicles,
effects, HUD, menus, and other screen-space layers. A visually plausible
track-only image must not be mistaken for complete triple-screen support.

### Experimental runtime integration (work in progress)

The `codex/triple-wheel-integration` branch combines this renderer with the
wheel/launcher branch. Its launcher retains the wheel bindings, FFB and
telemetry mods and adds Triple Screen as a separate experimental toggle. A
7680×1440 Surround smoke test rendered a center race view with distinct left
and right ground panels; the regular Stream Deck launcher was not replaced.

The launcher now exposes an opt-in **Triple Screen (experimental)** mod. Its
SDL presenter accepts only a 7680×1440 fullscreen Surround surface with no
shader selected, draws the normal game compositor in the center 2560×1440
panel, and evaluates separate 512×288 ground rays for each side panel. Menus,
save-state and rewind overlays stay centered. A rejected camera calibration,
wrong display mode, or non-race scene falls back to the centered game view.
The runtime side compositor now samples the captured Mode 1 BG1/BG2 panorama
at each panel's horizontal eye angle and carries its skyline down to the
ground-plane horizon. This removes the black sky/gap seen in the first rig
preview, but side vehicles and effects are still missing. This is **not** complete
triple-screen support and must not replace a working install without a rig
test. The physical values are currently a pinned copy of the saved rig profile;
the versioned toolkit layout/status adapter has not yet been connected.
On one captured active-race frame, both live side buffers matched the offline
reference byte-for-byte before the sky pass. In an 11,364-frame recorded-drive
replay with 7680×1440 Surround, the sky-enabled experimental build completed
and suppressed two near-white impact frames. Its measured race composition
averaged 4.48 ms and the old combined side-projection/texture-upload/draw
submission bucket averaged 8.33 ms; 272 presentations were
missed, versus 46 in a prior ground-only replay. Runs were not simultaneous,
so this is a performance warning rather than a controlled A/B benchmark.
The current diagnostics split side projection and texture upload into their
own buckets, but no 7680×1440 Surround trace using that split exists yet.
The source-tile atlas reuses the BG1/BG2 panorama through horizontal scroll
changes and validates the exact VRAM words it sampled. On one fixed 640×360
capture, a controlled 200-iteration CPU comparison measured 13.07 ms with
direct skyline sampling and 11.70 ms with the atlas; their output files were
byte-identical. This isolated speedup does not establish a 60 Hz rig result:
subsequent live replays ran alongside other desktop windows and had variable
presentation pacing. The normal install remains untouched.
Additional private replay captures at frames 2,000, 4,000, 6,000, 8,000, and
10,000 produced byte-identical cached/direct side panels. Five-iteration
640×360-per-panel CPU timings were 12.6–12.8 ms with the atlas versus
14.0–14.2 ms with direct skyline sampling. Both paths correctly rejected
frame 11,200's non-race camera. These are offline side-buffer results, not a
new Surround presentation test.
An opt-in `FZERO_TRIPLE_ATLAS_AUDIT=1` headless replay now keeps the atlas warm
through every race source frame and compares both complete 512×288 side
buffers with direct panorama sampling every 60 frames. The 11,364-frame
recorded drive passed: 164 comparisons, zero pixel mismatches. Its one earlier
rejection was a captured countdown frame trailing the guest's race-state
transition. Side projection now accepts that live countdown track; the same
replay has zero rejections and still 164/164 matching comparisons. A ROM-free
fixture checks the countdown case. This does not validate SDL or physical
Surround presentation.
With `FZERO_TRIPLE_SKY_TRACE=1` on the same verified drive, the atlas reported
one initial miss and at least 9,840 hits, with zero side-output rejections.
Repeated atlas construction is therefore not an apparent source of the
recorded presentation misses. Retaining the temporary sky-colour buffer across
frames was tried and discarded: two 300-iteration captured-frame CPU runs
changed from 7.28/7.21 ms to 7.24/7.27 ms, below a convincing improvement,
while producing byte-identical output. These are isolated CPU measurements;
the live GPU/upload/present costs still need a new Surround trace.
The ground intersection now evaluates one rational projection per physical
panel row instead of a normalized ray at every side pixel. Nine private race
and impact captures remained byte-identical to the prior renderer. Alternating
200-iteration offline A/B measurements at 640×360 per side reduced side
composition from roughly 11.94–11.97 ms to 11.31–11.42 ms per frame. The
`FZERO_TRIPLE_DISABLE_ROW=1` diagnostic switch retains the direct path for
future pixel comparisons; this CPU result is not a new display-pacing result.
The side skyline's horizon is now solved on each physical panel plane. At the
saved rig's centered eye position this is byte-identical on five sampled race
frames. A ROM-free raised-eye fixture (120 mm above panel center) shows why
the exact solution matters for other layouts: interpolating normalized top
and bottom rays misses the true horizon by over nine output pixels on an
angled panel. That fixture validates the horizon equation, not complete
raised-eye calibration or presentation support.
An offline three-panel composite of captured frames 1800 and 2777 used the
runtime-capture tool's actual 640×360 side buffers and a matching scaled
center image. At frame 1800, the green track-edge marker meets the center
within about 2–3 output pixels at both inner side edges. An attempted
ground-coordinate seam warp changed much of the side texture for only a tiny
edge improvement, so it was discarded. This is an offline visual/marker check,
not proof of physical bezel alignment at the runtime's 512×288 side resolution.
At captured crash frames 6143–6144, the side output is white along with the
guest center image; this is the same original-game impact flash described in
`PLAYTHROUGH_RECORDING.md`, not evidence of a side-only renderer fault.
The normal runtime no longer precomputes normalized rays for every side-panel
pixel; it retains only top/bottom edge rays for skyline work. At the current
512×288-per-side runtime resolution, that reduces the ray cache from about
6.75 MiB to 48 KiB. The full ray cache is allocated only if the offline
`FZERO_TRIPLE_DISABLE_ROW=1` comparison path is selected. Five race captures
remained byte-identical; one 640×360 offline first-render measurement fell
from about 17 ms to 13 ms, while steady composition stayed near 11.3 ms.
At the actual 512×288 runtime side-buffer size, two captured race frames
(1800 and 6000) produced byte-identical outputs with the atlas and rational
row projection enabled or disabled. In 200-iteration isolated CPU runs, the
normal path took 7.34/7.49 ms per side-buffer pair, versus 8.52/8.59 ms
without the sky atlas and 7.90/7.92 ms without the row projection. These
measure composition only, not upload, GPU draw, presentation or live pacing.
The SDL presenter now retains the uploaded side texture when another host
presentation reuses the same projected emulated frame. A ROM-free test confirms
the projection version remains stable on a cache hit and advances when the
output is rewritten; the full 11,364-frame recorded replay and 13/13 tests
pass. This avoids redundant texture uploads at high refresh rates, but has not
yet been measured for presentation pacing in 7680×1440 Surround.

The initial test fixture uses the locally saved rig measurements: three
2560×1440 panels, 708.42 mm visible chord width, 398.48 mm height, 660 mm eye
distance, 8 mm bezel gap, and 70° left/right yaw. The projection math mirrors
the renderer-neutral eye-ray API added in private `dbce-triple-screen-toolkit`
revision `63b7c558645a4851416d7c2807c86fa56ffe9259`, with bezel spacing
added at this adapter boundary. The private toolkit is not a public build
dependency. Curved panels are currently approximated by their visible chords.

Active-race captures at simulation frames 1600, 1650, 1700, 1800, and 1900
(including a sustained steering input) fit the two-line ground model over
scanlines 60–210: horizontal scale error stays within 0.49%, and the
recovered scanline center differs by at most 1.2 texture units. Frame 1600
also exposed a required longitudinal scale factor (0.4322 in that frame); assuming
isotropic world units produced errors up to 375 texture units. The corrected
factor has a captured-line regression fixture in the C test. This validates a
small sample of race frames, not every scene or camera transition. The next
gate is colour/pixel comparison across more tracks, effects and camera
transitions. The experimental runtime remains opt-in and falls back to the
center view wherever camera calibration fails.

### Offline ground-render evidence

The offline prototype rendered frames 1600, 1650, 1700, 1800, and 1900 from
recorded race/steering input at 640×360 per panel. Every frame produced three
different ground sightlines and a 100% center-panel **texture-coordinate**
match against its original Mode 7 scanlines over rows 80–210 after per-row
alignment. Frame 1800 also rendered at the rig's full 7680×1440 Surround
resolution (three 2560×1440 panels) with the same center-coordinate match.
These numbers establish texture sampling, not complete scene composition or
visual equivalence where sprites, HUD, and sky are present. Frame 1300 was
rejected as a transition/non-race view rather than drawn through a misleading
camera calibration.

The pure panel/calibration test needs no ROM or initialized submodules:

```text
cmake -S . -B build-triple-math -DFZERO_BUILD_GAME=OFF
cmake --build build-triple-math --target fzero_triple_geometry_tests
ctest --test-dir build-triple-math -R fzero_triple_geometry
```

With a private verified ROM capture and the usual game build prerequisites,
the offline tool can be built and run separately from the game executable:

```text
cmake --build build-dev --target FZeroTripleGroundCapture
build-dev/FZeroTripleGroundCapture captures/race/frame-001800.bin triple.ppm 708.4166 398.4843 660 70 70 8 640 360
```

The output is ignored under `captures/` during local testing. The tool
validates active-race Mode 7 lines, rejects unsupported camera pitch, requires
three distinct ground projections, and fails if fewer than 99% of the center
texels align with the captured affine renderer.

Consequently:

- A wider viewport is useful, but it is not three independent projections.
- A side-panel yaw cannot be represented by changing the existing Mode 7
  `origin + x * step` transform. A rotated physical panel produces a
  projective mapping (the ray/ground-plane denominator varies across X).
- The host renderer has enough information to prototype a true physical
  projection for the Mode 7 ground because it samples the captured world plane
  per output pixel and can resolve tiles outside the retail 1024x1024 streamed
  square from the reconstructed course.
- The host renderer does not currently have a complete world-space description
  of every sprite. It identifies and widens the six vehicle reservations, but
  most effects, trackside objects, and HUD elements remain screen-space OAM.

The honest first capability is therefore **three panel-correct ground
projections with a center-only SNES compositor**, marked degraded while
world-space sprite coverage is incomplete. It must not advertise three fully
independent cameras until frame evidence confirms that all scene layers obey
the three projections.

## Renderer evidence

The relevant seams are:

- `src/fzero_mode7.h`: `FzeroMode7Line` is a per-scanline affine transform;
  `FzeroMode7Locate()` evaluates it once per sample.
- `src/fzero_renderer.c`: the HD path already samples Mode 7 per output pixel,
  reconstructs the full course beyond the guest tilemap, and separates Mode 7,
  BG3, OAM, windows, and colour math.
- `object_owner()` recognizes the six vehicle reservations. This is enough to
  investigate world-space reprojection for racers, not enough to claim general
  sprite reprojection.
- `race_hud` already distinguishes HUD anchoring from the world. Triple-screen
  mode should render the guest UI once in the center panel rather than repeat or
  stretch it across the span.

No 4x4 camera/view/projection matrix exists to patch. A reusable toolkit matrix
can describe panel geometry, but a Mode 7-specific adapter must convert each
panel pixel into a ray and intersect that ray with the reconstructed track
plane.

## Projection model

Treat the player eye as the origin. For each planar monitor, derive its center,
right, and up vectors from the measured panel size, eye distance, eye height,
and panel yaw. For an output pixel `(u, v)` on panel `p`:

1. Convert `(u, v)` to millimetres on the panel surface, including any physical
   bezel gap.
2. Form the eye ray through that point.
3. Transform the ray into F-Zero camera coordinates using the captured camera
   heading and the calibrated relationship between the emulated horizon and
   eye height/pitch.
4. Intersect the ray with the flat Mode 7 ground plane.
5. Convert the intersection into the course's periodic world coordinates and
   sample via the existing course/VRAM path.

The center panel should be calibrated to match the stock renderer at its center
and horizon. That gives a regression oracle and avoids inventing a new driving
view. Left and right panels then use the same eye and distinct panel planes.

This is deliberately a per-pixel mapping. Fitting each panel to one widened
affine Mode 7 line would only approximate the center of a panel and would bend
or shear geometry toward its outside edge.

## DBCE contract boundary

The desired-layout input is the version-1 `triple-screen-layout` contract. A
future adapter should:

- reject unknown `schemaVersion` values;
- support `nvidia-surround` and `borderless-span` first;
- defer `separate-displays` until the SDL presentation layer can own and
  synchronize three windows;
- atomically publish a version-1 runtime status containing the accepted layout
  SHA-256 and observed frame state;
- publish `activeCameraCount: 3` only when all three distinct ground projections
  were rendered in the latest successful frame;
- use `state: degraded` plus a stable diagnostic such as
  `SCREENSPACE_SPRITES` while sprite layers do not share those projections;
- list `centered-ui` only after the center-only compositor is active;
- list `three-projections` only for actual independently evaluated panel rays,
  never for a single wide affine viewport.

The toolkit is currently private. The public F-Zero repository must not add it
as a Git submodule or build dependency. Until it is published, either consume a
reviewed, pinned generated C header/artifact containing only the required panel
math, or implement the small contract reader in the external adapter. Record
the toolkit revision and conformance tests when that boundary is introduced.

## Atomic implementation plan

Keep this work independent of analog input, force feedback, and telemetry.

1. **Math/capture prototype**
   - Done offline: panel-ray calculator, captured-line calibration, three
     panel-correct track-only images, and ROM-free regression tests.
   - Pending runtime: one borderless Surround span, debug dividers, yaw hot
     reload, invalid-camera fallback, and frame-by-frame stock comparison.
2. **Center compositor**
   - Composite BG3, HUD OAM, windows, and menus only in the center panel.
   - Keep menus and non-race scenes centered and unmodified.
3. **World objects**
   - Reproject the six known vehicle owners from WRAM/world state.
   - Inventory effects and trackside objects by OAM writer; reproject only when
     a stable world owner is proven.
   - Retain `degraded` status for any unsupported world-space layer.
4. **Adapter/status**
   - Validate desired-layout JSON and hash its exact accepted bytes.
   - Publish atomic runtime status with frame evidence and diagnostics.
   - Add manifest capabilities only as each stage is verified.
5. **Separate windows (optional)**
   - Add three synchronized SDL windows only after span mode is correct.

## Acceptance tests

- Zero-yaw center projection matches the existing HD ground renderer.
- A straight track seam crosses monitor boundaries without a heading change.
- Equal left/right yaw produces mirror-symmetric panel geometry on a symmetric
  rig.
- Bezel width removes the corresponding physical view wedge instead of merely
  covering pixels.
- HUD and menus appear once, centered, and retain their stock aspect.
- Runtime status never reports three active cameras for a stretched/widened
  affine frame.
- Invalid or newer layout contracts are rejected without changing the last
  known-good renderer configuration.

## Reusable-toolkit feedback

The private toolkit now exposes a renderer-neutral `EyeRayCalculator`, which
this prototype mirrors rather than inventing a 4x4 camera matrix for Mode 7.
The remaining interoperability work is a stable native C ABI (or generated
artifact), explicit bezel-gap handling, and conformance fixtures shared with
the toolkit. Until that boundary is public and tested, the game fork should
retain only this small, pinned source adapter rather than a private submodule.
