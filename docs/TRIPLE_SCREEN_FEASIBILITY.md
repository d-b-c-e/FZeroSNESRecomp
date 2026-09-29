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

## Prototype status (2026-09-27)

`src/fzero_triple_geometry.*` now calculates distinct eye rays for three
physical panels, including the bezel gap. `src/fzero_triple_ground.*` is an
experimental flat-ground calibration from two captured Mode 7 scanlines. A
deterministic test checks panel symmetry, independent sightlines, and that
center-panel ground rays reproduce synthetic source scanlines. The separate
integration branch now has an opt-in three-panel ground compositor, but the
normal installed game still uses one wide view. Side-panel sprite reprojection
is not active yet.

`FZeroTripleGroundCapture` is an offline renderer built from immutable capture
files. It uses the existing course-table/VRAM lookup and colour pipeline to
write three distinct ground panels into one PPM span. A per-scanline similarity
correction matches the center panel's exact captured Mode 7 step and origin
while retaining distinct physical rays on the side panels. It does not run in
the game, and deliberately renders sky black and omits all vehicles, effects,
HUD, menus, and other screen-space layers. A visually plausible track-only
image must not be mistaken for playable triple-screen support.

### Experimental runtime integration (work in progress)

The `codex/triple-wheel-integration` branch combines this renderer with the
wheel/launcher branch. Its launcher retains the wheel bindings, FFB and
telemetry mods and adds Triple Screen as a separate experimental toggle. A
7680×1440 Surround smoke test rendered a center race view with distinct left
and right ground panels; the regular Stream Deck launcher was not replaced.

The launcher now exposes an opt-in **Triple Screen (experimental)** mod. Its
SDL presenter accepts only a 7680×1440 fullscreen Surround surface with no
shader selected, draws the normal game compositor in the center 2560×1440
panel, and evaluates separate 640×360 ground rays for each side panel. Menus,
save-state and rewind overlays stay centered. A rejected camera calibration,
wrong display mode, or non-race scene falls back to the centered game view.
The sides still have dark sky and no vehicles/effects. This is **not** complete
triple-screen support and must not replace a working install without a rig
test. The physical values are currently a pinned copy of the saved rig profile;
the versioned toolkit layout/status adapter has not yet been connected.
On one captured active-race frame, both live side buffers matched the offline
reference byte-for-byte. Cached rays and per-row alignment reduce the two
640×360 side projections to about 10 ms of single-thread CPU time per frame
on the development rig; center composition, uploads, and presentation are
additional costs. This is a benchmark, not an on-rig frame-time guarantee.

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
transitions, with an explicit fallback wherever the calibration fails. Until
then, keep the new module disconnected from the runtime renderer.

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
