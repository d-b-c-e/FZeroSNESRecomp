# Changelog

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
