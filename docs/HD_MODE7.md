# HD Mode 7

Enable **Mods -> HD Mode 7** in the launcher and choose **2x** or **4x**.
The default is disabled. It works at 4:3 and with Widescreen, independently
of Presentation FPS. At 4x, each original track pixel gets 16 new samples
from the game's tilemap. Cars, HUD and menus retain their pixel artwork.

The settings are saved in `fzero-video.ini` next to the executable:

```ini
HDMode7=1
HDMode7Scale=2
```

The enhanced track uses more precise affine coordinates and interpolates
between adjacent scanlines of the same camera band. It preserves the native
frame for snapshots/thumbnails and leaves game timing, WRAM and CPU execution
unchanged. Palette effects and window masks keep their per-scanline timing.

Mosaic, EXTBG, direct colour and interlaced/hires combinations retain the
existing image at the chosen scale. Title/menu groups also retain their
existing composition. This first integration enhances race/attract track
scenes; it does not replace Super FX rendering or supply higher-resolution
sprite artwork. It is an original implementation, not imported bsnes code.

4x costs more rendering time than 2x, especially at wide aspect ratios and
high presentation FPS. Start at 2x. Both SDL and OpenGL/shader presentation
receive the actual enlarged texture; display aspect ratio stays the same.

For offline checks, `FZeroRenderCapture` accepts `FZERO_HD_SCALE=2` or `4`.
Unset it for the original output. Its existing sequence mode also exercises
frame interpolation. The renderer tests cover additional source detail,
buffer capacity, unchanged native output, scanline interpolation and splits,
and exact menu fallback. The launcher/config tests cover the independent
toggle, saved resolution and rejection of invalid settings.

When developing alongside the framework, configure `SNESRECOMP_ROOT` to point
to its matching HD Mode 7 worktree. No ROM data or generated captures belong
in the source repository.

## Validation of this change

- Windows Release/SDL3 build, including BS Deluxe: all nine CTest tests pass.
- Native output from a recorded BS Deluxe race frame matches the pre-change
  renderer byte for byte. Its 4x HD output was visually compared with native.
- Desktop SDL dummy-driver runs with HD disabled/enabled reach identical
  128-KiB WRAM after a 1,500-frame menu route and a 1,600-frame race route.
  The race comparison uses 16:9 and 2x HD; the menu run uses 4:3.
- The race run reported 10 missed presentations out of 1,600 with HD enabled
  and zero with it disabled. These are smoke checks on a shared development
  machine, not controlled performance measurements. HD remains opt-in.
- OpenGL presentation builds successfully; live OpenGL/shader validation
  remains outside these checks.
