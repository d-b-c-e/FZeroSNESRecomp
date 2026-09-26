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
   - Add a renderer-only panel-ray calculator with deterministic tests.
   - Render three panel-correct Mode 7 ground images into one borderless span.
   - Add a debug divider and per-panel yaw hot reload.
   - Compare a zero-yaw, center-panel render pixel-for-pixel with the current HD
     Mode 7 path.
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

The shared toolkit would benefit from a renderer-neutral API that returns
world-space eye rays (or panel plane basis vectors) in addition to conventional
off-axis 4x4 matrices. That API would let raycasters, 2.5D floor renderers, and
Mode 7-style engines consume the same measured rig geometry without fabricating
a 3D camera matrix. A conformance fixture for symmetric rigs and bezel gaps
would make vendored generated artifacts straightforward to verify.
