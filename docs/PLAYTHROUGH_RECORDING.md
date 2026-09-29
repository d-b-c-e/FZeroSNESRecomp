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
delivered wheel torque. Visual frame checks and a shared toolkit force-signal
schema are follow-on work. The binary `.fzpt` is intentionally a F-Zero input
adapter, not a replacement for Cruis'n Collection's MAME INP format.
