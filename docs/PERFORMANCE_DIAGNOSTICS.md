# Performance diagnostics

Open the launcher, choose **Mods > Diagnostics**, enable it, and press Play.
Play through the slowdown for a few seconds, then quit normally. Attach the
newest `diagnostics/performance-*.jsonl` file to your report and describe where
the slowdown happened. Include the approximate time if possible.

The folder is beside the executable on Windows or an unpacked Linux build,
and beside the AppImage on Linux. Each session gets its own timestamped file.
Diagnostics is **off by default**. Disable it in Mods when finished; that
stops creating new reports. Existing reports remain available. If Skip Launcher
is enabled, start the executable with `--launcher` to reach the toggle.

Logs stay local and are never uploaded automatically. They include build and
dependency versions, CPU/OS/RAM, graphics backend and available adapter/driver
information, shader preset name, active settings and periodic timing summaries.
They do not include ROM contents, saves, personal file paths, or player names.
An unwritable diagnostics folder does not stop the game.

## Reading a report

- `session` identifies the build, backend and allocated texture scale. OpenGL
  includes the actual renderer/vendor/version and swap interval. For SDL/D3D,
  Windows also lists attached display adapters; these are not proof of which
  adapter rendered a frame. `vsync=-99` means the query was unavailable.
- `settings` records the initial settings. `sample` records interval deltas
  about every two seconds. `final` flushes the last partial interval on exit.
  Settings in a sample describe its end, so resizing or changing aspect/FPS
  during that interval can span more than one configuration.
- Compare `requested_scale`, `allocated_scale`, and `effective_scale`. The
  latter describes the framebuffer actually submitted, including fallback to
  native resolution before a valid HD frame exists. Source and output sizes
  distinguish HD sampling, widescreen width, and window/display resolution.
- `simulation_fps` should be near 60.099; `presentation_fps` is independent.
  The sample includes target/display rates, missed presentation deadlines,
  and frame interval p95/p99/max. Percentiles are upper bounds in 0.25 ms
  histogram bins; intervals above 128 ms use the observed maximum.
- `stages` contains calls, total, mean and maximum **main-thread wall time**:
  simulation, native PPU capture, presentation composition (including HD Mode
  7), texture upload, draw submission, present/swap, pacing wait, and pause.
  Means use the stage's call count, not the simulation frame count.
- `present` includes driver/vsync waiting; `draw_submit` is CPU submission
  time, not a GPU execution measurement. No GPU fences are inserted. A large
  upload/present time can reflect earlier asynchronous GPU work. Overall CPU
  or GPU utilization alone cannot identify the limiting stage.
- In the experimental triple-screen SDL path, `triple_projection` measures
  the two CPU side-panel buffers and `triple_upload` measures their texture
  upload. These stages are zero outside that path. Older reports included both
  operations in `draw_submit`, so their draw timings cannot be compared
  directly with reports from this build.
- `unattributed_ms` covers input, audio-lock interactions outside measured
  stages, bookkeeping, menus, logging and other work. Simulation time already
  includes any waits inside simulation. Pause, menu and state-action events
  help identify discontinuities; a long menu can span a sample interval.

For repeatable summaries without sharing private JSONL files, run
`py -3 tools/summarize_triple_perf.py diagnostics/performance-*.jsonl`. It
selects race-scene samples, weights stage means by call counts, and reports the
largest missed-deadline intervals. A sample's scene is its **end** state, so
race entry/exit intervals can straddle another scene. The tool labels old
combined draw timing and new split timing separately; those draw means must
not be compared as if they measure the same work.

On the two older 59.95 Hz Surround recordings used for the sky/ground
comparison, the sky-enabled drive had 269 missed deadlines in race-labelled
samples versus 46 in the ground-only drive. Their combined draw means were
8.324 and 6.578 ms/call, while composition means were 4.477 and 4.495 ms.
The runs were not simultaneous and include CPU-side projection inside the old
draw bucket, so these numbers flag a pacing risk, not a GPU bottleneck proof.
A complete replay with the newer split-stage diagnostics is still needed.

Instrumentation adds a small amount of work while enabled. With the option
off there are no diagnostic files, allocations or performance-counter reads
from the logger. The existing crash-report system is independent.
