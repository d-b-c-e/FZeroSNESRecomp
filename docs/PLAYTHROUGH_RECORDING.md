# Recorded F-Zero drives (experimental)

This opt-in diagnostic records the **SNES input word after wheel/keyboard
binding**, once per emulated frame. It writes a complete machine snapshot
before frame zero and a post-frame WRAM hash for every input. The recording
contains no ROM. No physical wheel is opened during headless replay.

Use the experimental executable, not the normal Stream Deck install. In
PowerShell, choose a new path in an existing folder; the recorder refuses to
overwrite a prior case:

```powershell
$env:FZERO_RECORD_PLAYTHROUGH = 'E:\Source\fzero-triple-wheel-integration\build-integration\rig-preview\diagnostics\my-drive.fzpt'
& 'E:\Source\fzero-triple-wheel-integration\build-integration\rig-preview\FZeroSNESRecomp-triple-preview.exe'
```

Play normally, then quit normally. The `.fzpt` and `.fzpt.state` files are a
pair. A crash or forced termination leaves the input file marked incomplete;
replay refuses it. Opening the save-state or rewind overlay, loading a state,
or resetting during the recording also invalidates it, rather than producing
a misleading replay. Use a unique name for the next attempt. The recorded
input is game-visible digital input, not raw wheel samples, so it reproduces
the route but cannot test a different steering calibration or closed-loop FFB
feel.

For offline replay, use the headless build and the same verified ROM and
cartridge mode/MSU patch **and center-screen viewport** as the recording. A
Surround triple-screen recording uses a 16:9 center viewport; leaving headless
at its stock 4:3 default diverges when the first race scene is drawn. The
cartridge SHA-256 is checked
before applying any input; each emulated frame's WRAM hash must match. Set
`SNESRECOMP_MSU1` to the original pack path if the case was recorded with
enhanced music (otherwise leave it unset):

```powershell
$env:FZERO_REPLAY_PLAYTHROUGH = 'E:\Source\fzero-triple-wheel-integration\build-integration\rig-preview\diagnostics\my-drive.fzpt'
$env:FZERO_FFB_MODEL_TRACE = '1'
$env:FZERO_ASPECT = '16:9'
$env:SNESRECOMP_MSU1 = 'E:\Source\fzero-triple-wheel-integration\build-integration\rig-preview\music\F-Zero DX Expanded (JUD6MENT)'
& 'E:\Source\fzero-triple-wheel-integration\build-integration\FZeroSNESRecompHeadless.exe' 'E:\Source\fzero-triple-wheel-integration\build-integration\rig-preview\fzero.sfc'
```

The force trace prints spring, damper, road, and collision requests without
calling the wheel DLL. A matching input/WRAM replay proves repeatable game
simulation for that case; it does **not** prove identical GPU presentation or
delivered wheel torque. Automated GPU frame comparison and a shared toolkit
force-signal schema are follow-on work. The binary `.fzpt` is intentionally a F-Zero input
adapter, not a replacement for Cruis'n Collection's MAME INP format.

To replay the same drive through the experimental SDL/Surround renderer,
set `FZERO_REPLAY_PLAYTHROUGH` to the case and run the experimental executable
with the ROM path as its argument. Keep the recorded video's 16:9 center,
MSU mode and triple-screen settings. This path validates every WRAM checkpoint,
exits when the drive ends, does not write SRAM, and **never initializes the
physical FFB device**. `SDL_AUDIODRIVER=dummy` silences the test without
changing the game's audio-consumer timing.

The optional `Mods → Reduce crash flashes` setting (or one-run
`FZERO_SUPPRESS_RACE_FLASH=1`) holds the last displayed frame during a brief,
near-white race-impact flash. The game and FFB still advance. It is off by
default, and the environment variable `0` overrides an enabled launcher
setting for A/B comparison.

For a device-free source check, set `FZERO_LUMA_TRACE=1` on the headless replay.
It samples source pixels before SDL and logs bright race frames, brightness,
and energy. On the 11,364-frame rig recording, the two near-white frames at
headless indices 6142–6143 were already present in the guest compositor:
the captured image shows a white background with an explosion and HUD, not a
Surround-only rendering fault. The SDL flash guard held these same two frames.
`FZERO_TRIPLE_SIDE_LUMA_TRACE=1` additionally composes both side panels at the
runtime's 512×288 resolution on every race frame and reports bright or sharply
brighter side frames alongside the center mean. In the full verified replay,
headless frames 6142–6143 measured left/right 255/255 and center 246/249.
The only other logged side jumps were moderate (frame 3001: 155/128; frame
6178: 118/147); no side-only near-white frame was detected. This rules out a
side-compositor white flash in this recording, not a later SDL/GPU/Surround
presentation artifact or a different playthrough.
One smaller luminance rise at 3025 was a track surface turning light grey and
persisting in following frames; it should not be suppressed as a flash.
With the original 32-unit impact threshold, device-free FFB model tracing
requested collision pulses at frames 3417, 5526, 6139, and 6169. A later
16-unit threshold also catches three isolated 24-unit losses at 3068, 3502,
and 10222. The full 11,364-frame headless replay passed with all seven model
pulses. This verifies model decisions, not that the wheel received or
reproduced the effects; physical validation remains attended work.

## Shared toolkit force-observation adapter

`tools/fzero_replay_adapter.py` consumes the toolkit's
`tools/replay/replay_case.py` contract without copying its implementation or
loading `WheelFfb.dll`. `create` pins the original `.fzpt`, snapshot, ROM,
captured executable, config and video settings, plus a local copy of the small
MSU IPS patch. New cases snapshot the settings so later launcher changes do not
rewrite the evidence. Keep the manifest and all referenced artifacts private;
the state and ROM are not committed to Git. The already captured September 28
case was made before settings snapshots were added; its original config/video
bytes were separately saved in `diagnostics/`, and those live files must retain
their recorded hashes for this manifest to validate.

The `.state` artifact must be **exactly** the `<source>.fzpt.state` file that
F-Zero's headless player loads. `create` rejects a different state path, and
`observe` checks the validated manifest path again before launching the game.
This is important even when another snapshot has a valid hash: hashing a file
the game does not load would not establish replay identity.

The September 28 case pins capture source commit
`0fe1e6452de2b182300ccb72e96080d9054f38ba` (tree
`cc41ec7118e019d57f848e84ebbf4a9b4adff5a9`) and the actual triple-preview
capture executable SHA-256
`55C495F790A27E782F0503871EF4C10F765CA2C62F48A320E2DE94E002BCA30D`.
Those were checked against the unchanged preview executable and source record;
the later headless observation runner is a **different binary**. The existing
case's `dirty:false` is a pinned build-time assertion, not something the
adapter can prove retroactively. For future cases, `create` conservatively sets
`dirty:true` unless `--capture-receipt` supplies a build-time JSON receipt with
exact fields `schema`, `version`, `sourceRevision`, `sourceTree`,
`executableSha256`, `dirty`; it must identify a local Git tree and match the
capture executable, with `schema="fzero.capture-build"`, `version=1`, and
`dirty=false`. The receipt is consistency evidence, not a cryptographic proof
that a binary came from a source tree.

`observe` validates artifact hashes, the ROM and MSU patch, and the rig's
recorded center viewport (`16:9` for triple-screen `Fit`). It runs the
**headless** game with exact SNES input and per-frame WRAM checks, then validates
the complete raw output before creating a toolkit observation stream. The
stream records actual emulated master-cycle ticks and ordered software
requests, with `physicalOutput=false`. Spring, damper and road are normalized
from DirectInput's 0-10000 request range. Historical v1 observations represented
impact as a 140 ms, 32 Hz event edge; v2 uses the configured cue described below.
The profile assumes supported spring/damper/road slots, so the fallback
constant-force request is zero. This is not a measurement of wheel torque.
BS Deluxe cases are explicitly refused for now because their separate Deluxe
data must also be pinned before replay can claim the same game identity.

For the existing drive, from this repository root:

```powershell
py -3 tools/fzero_replay_adapter.py observe `
  --case 'build-integration\rig-preview\wheel-drive-20260928-235320.case.json' `
  --toolkit 'C:\Users\antho\.codex\worktrees\recorded-playback\dbce-wheel-mod-toolkit' `
  --runner 'build-integration\FZeroSNESRecompHeadless.exe' `
  --rom 'build-integration\rig-preview\fzero.sfc' `
  --output 'build-integration\rig-preview\diagnostics\next-observation.jsonl' `
  --strength 12
```

Use a new output name for each run. `--strength` can vary between trials while
the original case SHA-256 stays fixed. Validate a case with the toolkit's
`replay_case.py validate` command and compare two observations with its
`compare` command (exit 1 means expected differences, not a replay failure).
The September 28 case has SHA-256
`aaec60f5e370734536820accc262c0a9c7a268fdc19d594f90e63db8332720ce`.
Before the threshold adjustment, strengths 12 and 20 both passed 11,364-frame
replays, each emitted 45,461 ordered requests, and the four impact edges
remained at frames 3417, 5526, 6139 and 6169. The toolkit comparison found
29,083 changed requests with the same case identity. These software
comparisons do not prove FFB delivery or
physical feel; that still requires an attended wheel test.

## Independent crash trials (software model v2)

The older v1 adapter tied impact magnitude to centering and capped it at 35%,
always describing a 140 ms sine cue. The current launcher instead has independent
ImpactStrength and a Constant/Sine choice. New observations use model v2 and
read those settings from the pinned config, with the current native defaults
of 20% and Constant when absent. Override only the trial with
`--strength 12 --impact-strength 50 --impact-type Constant`; compare it with
an 80% impact trial using a new output filename. The recording and centering
requests stay identical. Constant requests are 120 ms; sine comparison requests
are 32 Hz for 140 ms. The trial/config and profile hashes identify these choices;
old observations remain historical and are never rewritten.

This adapter emits configured software intent. It does not inspect or load the
actuator DLL, establish constant-burst support, model a hardware fallback, or
prove delivered wheel torque. Run only the device-free headless runner for these
trials. Stronger requested collisions do not establish wall-hit classification
or severity scaling; those require additional evidence.
