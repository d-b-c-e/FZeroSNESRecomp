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

`observe` validates artifact hashes, the ROM and MSU patch, and the rig's
recorded center viewport (`16:9` for triple-screen `Fit`). It runs the
**headless** game with exact SNES input and per-frame WRAM checks, then validates
the complete raw output before creating a toolkit observation stream. The
stream records actual emulated master-cycle ticks and ordered software
requests, with `physicalOutput=false`. Spring, damper and road are normalized
from DirectInput's 0–10000 request range; impact is a 140 ms, 32 Hz event edge.
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
At strengths 12 and 20, both 11,364-frame replays passed, each emitted 45,461
ordered requests, and the four impact edges remained at frames 3417, 5526,
6139 and 6169. The toolkit comparison found 29,083 changed requests with the
same case identity. These software comparisons do not prove FFB delivery or
physical feel; that still requires an attended wheel test.
