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
averaged 4.48 ms and GPU draw submission 8.33 ms; 272 presentations were
missed, versus 46 in a prior ground-only replay. Runs were not simultaneous,
so this is a performance warning rather than a controlled A/B benchmark.
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
The normal runtime no longer precomputes normalized rays for every side-panel
pixel; it retains only top/bottom edge rays for skyline work. At the current
512×288-per-side runtime resolution, that reduces the ray cache from about
6.75 MiB to 48 KiB. The full ray cache is allocated only if the offline
`FZERO_TRIPLE_DISABLE_ROW=1` comparison path is selected. Five race captures
remained byte-identical; one 640×360 offline first-render measurement fell
from about 17 ms to 13 ms, while steady composition stayed near 11.3 ms.

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
