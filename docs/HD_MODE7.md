# HD Mode 7

Enable **Mods -> HD Mode 7** in the launcher and enter a whole-number
resolution multiplier from **2 to 10**, including 6x, 8x and 10x.
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

**Warning: above 4x is extremely demanding and can cause severe slowdown.
Use at your own risk.** The launcher shows this warning when a higher value
is selected. Work and frame memory grow with the square of the multiplier:
10x samples 100 subpixels per original pixel, 6.25 times as many as 4x.
At maximum width, the 10x CPU frame alone uses about 58.4 MiB, plus graphics
textures and other game memory. Invalid config values revert to 2x; the input
is bounded at 10. If the renderer cannot create the requested texture, it
reports a lower resolution for that session without changing your saved choice.

Presentation FPS also multiplies the rendering work. Its Auto setting follows
the display's refresh rate, so a 120/240 Hz display requests more HD frames
than a 60 Hz display. For a slower machine, keep Widescreen enabled and start
with HD Mode 7 at 2x and Presentation FPS at 60. Wider views and 4x still need
more CPU time. See [performance measurements and reproduction](HD_MODE7_PERFORMANCE.md).

For offline checks, `FZeroRenderCapture` accepts `FZERO_HD_SCALE=2` through `10`.
Unset it for the original output. Its existing sequence mode also exercises
frame interpolation. The renderer tests cover additional source detail,
buffer capacity, unchanged native output, scanline interpolation and splits,
and exact menu fallback. The launcher/config tests cover the independent
toggle, saved resolution and rejection of invalid settings.

When developing alongside the framework, configure `SNESRECOMP_ROOT` to point
to its matching HD Mode 7 worktree. No ROM data or generated captures belong
in the source repository.

## Custom scale validation

- All nine Windows and Linux CTest suites pass, including integer scales
  2 through 10, buffer bounds and rejection of invalid values.
- The actual launcher accepts a typed `10`, shows the warning above the
  input, and remembers it after Quit/relaunch. This also passes with the
  external state directory used by AppImages.
- Recorded 32:9 race frames at 6x, 8x and 10x preserve every common 2x sample
  at interpolation alpha 0.5. The 10x output is 6820 by 2240.
- Windows SDL runs at 2x and 10x reach identical WRAM after 120 frames for
  stock and Deluxe with identical initial saves. Linux OpenGL with the Sharp
  shader also completes a 120-frame 10x run under Xvfb. These are functional
  checks; high scales are not a promise of real-time performance.

## Original HD integration validation (v1.8.0)

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
