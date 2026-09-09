# Vulkan and DLSS 5 experiment

This branch adds explicit SDL3 Vulkan presentation and optional live DLSS 5
Neural Rendering to F-Zero. The neural stage uses a separate Windows D3D12
process with CPU frame transfers; this is not native Vulkan DLSS integration.

## Run

Build with SDL3 using the normal CMake targets. Run
`tools/setup_dlss_experiment.ps1 -Build <build-directory>` once to install the
pinned bridge, Ampere runtime, and isolated Python dependencies. The setup
script verifies download hashes and writes `dlss-deps/manifest.json`.

Open the game launcher, enable **Mods > Vulkan (experimental)**, and set its
**DLSS 5 Neural Rendering** option to **On**. These settings persist in
`fzero-video.ini`. Vulkan alone requires neither Python nor NVIDIA hardware.

Alternatively:

```powershell
./tools/launch_vulkan.ps1 -Build ./build-experiment -Dlss
```

Supply `-Rom <path>` to skip the launcher. Ctrl+F8 toggles neural rendering
during play. Existing controls, save slots, audio and original simulation
timing remain available. The OpenGL shader preset is inactive under Vulkan.

Overrides: `FZERO_OUTPUT_METHOD=Vulkan` selects Vulkan, `FZERO_DLSS=0|1`
overrides the saved neural option. `FZERO_DLSS_PYTHON` and `FZERO_DLSS_ROOT`
override the dependencies under the executable's `dlss-deps` directory.

## Implementation and limits

- SDL3 owns the Vulkan device, swapchain, uploads and presentation. An explicit
  Vulkan request fails visibly if unavailable instead of silently using SDL's
  preferred driver. Original SDL and OpenGL paths remain selectable.
- The worker invokes NGX neural feature 18 through ComfyUI-DLSS5-NR v0.3.0's
  native C ABI. ComfyUI, Torch, DLSS Super Resolution and Frame Generation are
  not used. Upstream native bridge source is MIT licensed; the NVIDIA runtime
  is separate and is not committed or bundled in game releases.
- One frame may be in flight. Vulkan displays the newest completed neural
  frame while simulation continues. Worker errors restore original pixels;
  Ctrl+F8 off/on retries. Reset/load/resume and aspect changes invalidate old
  output. A Windows job bounds the worker and its Python child to game lifetime.
- Neural processing is 480 pixels high (width follows aspect, capped at 1280).
  Asynchronous presentation adds latency and repeats images between neural
  completions. The simulation/presentation counter is not the neural frame rate.
- On the tested 3080 Ti/driver combination, NVIDIA Optical Flow fails with
  `nvOFExecute 0x80004005` on the second temporal frame. Live mode deliberately
  resets neural history for each frame. Temporal stability is unproven and
  flicker/altered HUD lettering are possible. The offline `--temporal` flag
  retains the failing path for investigation; no synthetic motion is claimed.
- The pinned bridge's shutdown can hang on this driver. The disposable CLI
  terminates after flushing results; the host gives its worker two seconds
  before terminating its own isolated process/job. No driver or unrelated
  process is terminated.

## Reproduce validation

Use the Python interpreter at `<build>/dlss-deps/venv/Scripts/python.exe`:

```text
tools/run_vulkan_experiment.py --build <build> --rom <rom> --frames 1800 --dlss --screenshot <capture.bmp> --input-script 180:8,300:8,450:8,600:8,750:8,900:8,1000-1800:1
tools/capture_dlss_sequence.py --build <build> --rom <rom> --output <new-capture-directory>
tools/dlss_worker.py process --root <build>/dlss-deps/bridge/ComfyUI-DLSS5-NR --input <new-capture-directory>/frames --output <new-result-directory>
```

Omit `--dlss` for the original Vulkan control. Readback captures come from
SDL's Vulkan renderer, not desktop screenshots. Sequence export requires zero
stock-renderer pixel differences. Output reports record runtime hash, GPU,
settings, processing times and channel interpretation. Live evaluations are
logged to `dlss-deps/bridge/ComfyUI-DLSS5-NR/live.jsonl`.

The helper CLI intentionally runs experimental native code in its own process;
automation should apply an external timeout, as the bounded live runner does.

## Dependencies

- Native bridge v0.3.0, source `3745b8ab6c70761e8d9e7daf47948a389833086f`:
  https://github.com/lisitskyaa/ComfyUI-DLSS5-NR
- Ampere runtime `310.8.SF-v2`, DLL version `310.8.SF.0`, unsigned:
  https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2
- NumPy 2.2.6 and Pillow 11.3.0, isolated from the system Python environment.

Detailed machine-local evidence is in `build-experiment`; copyrighted captures,
ROMs, runtime DLLs and dependency archives remain untracked.

## Validated results (2026-09-08)

RTX 3080 Ti, driver 32.0.16.1686:

- Explicit Vulkan startup and 1,800-frame scripted race passed. Vulkan readback
  shows the game and HUD; original and neural captures were visually inspected.
- The matched original/neural race runs ended with identical WRAM SHA-256:
  `c971cd06f81ad60015d92d8d7b66cf8049cad9d7d6684367148ed5a85a1cb914`.
- Live neural output updated about 27-29 times/second while simulation remained
  near 60.099 Hz. Pooled warmed 640x480 evaluations: median 15.78 ms,
  p95 17.95 ms. These exclude upload/IPC/presentation and are not total latency.
- A 300-frame stock-aspect race sequence exported with zero differing stock
  pixels and processed through NR. Original, neural and side-by-side MP4s are
  local artifacts. These clips play cached results at the source rate; they
  are not recordings proving 60 FPS live neural rendering.
- Repeating the same 640x480 race frame produced identical output PNG hashes.
  A 1280x960 still also evaluated successfully. The modification is mostly
  shading/color at these settings; it does not reconstruct a modern 3D scene.
- 16:9 -> Fit -> 4:3 resizing passed, including neural resource size changes.
  Missing-runtime fallback completed normally with original Vulkan output.
  No live experiment worker remained after normal exit.
- All five CTest targets passed, including new renderer-option persistence
  coverage. The standalone Windows IPC module passes strict GCC syntax checks.

Temporal optical flow and its associated quality remain unresolved. This is a
playable experimental path, not a production renderer or a performance release.
