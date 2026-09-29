# Experimental WheelFfb override receipt — 2026-09-29

This identifies the **experimental rig preview only**. It is not a toolkit
release, a merge into toolkit master, or a physical force-feedback acceptance.
The installed Stream Deck/LaunchBox copy was not replaced.

## Source and binaries

- Source checkout: `E:\Source\dbce-wheel-mod-toolkit-actuator-fix`, branch
  `codex/fix-actuator-rearm`, commit
  `02fb715531f8c857f5d66c89382da3c724fde7d8`, tree
  `d224cba3e59f87c0a8680394cf8f7cb2d02900ec` (clean when built).
- The burst retrigger correction is `44ca79db9a6c438372780cf9624d3f173f93b6a4`;
  persistent-effect reacquire correction is the tip commit above. The unrelated
  condition coefficient threshold was returned to 500.
- Native API version: `GetWheelFfbVersion() == 900` (0.9.0). Each binary has 47
  named exports, verified with `dumpbin /exports` against the 47-name `.def`.
- x64 build: `native\wheelffb\build.bat`, SHA-256
  `F2D9DD3059A75ED381446C30488F5B75670642AF8972F2A2D593F983B9BE7972`.
  This exact DLL was copied to `build-integration\rig-preview\WheelFfb.dll`.
- x86 build: `native\wheelffb\build.bat x86`, SHA-256
  `DB9BAC4EE11A8DCCA0C6F43185C1060292F84C8E4448E699DC14F2FE61FDEE5A`.
  It was **not** installed into F-Zero.
- The previous experimental-preview DLL had SHA-256
  `A0100072FFA928B0CE78ACC891F6609A70A872F63C30103B3631EFE4617FA165`.
  This identity was logged before replacement; that binary was not retained
  as a separate file here. The normal installed copy remains untouched.

## Offline verification

Both architectures built and passed `build_burst_test.bat`,
`build_constant_burst_test.bat`, `build_pov_read_test.bat`, and
`build_logging_test.bat`. These compile the production adapter against fake
DirectInput effects/devices and never acquire hardware. `build_smoke.bat` also
compiled for both architectures; **smoke.exe was not executed**, since that
would require an attended wheel test. The x86 logging test emitted one C4389
signed/unsigned warning, with all assertions passing. A separate toolkit
review independently rebuilt and passed the burst and production POV/recovery
fake suites on both architectures.

The F-Zero preview executable was unchanged at SHA-256
`55C495F790A27E782F0503871EF4C10F765CA2C62F48A320E2DE94E002BCA30D`.
Its source commit is `0fe1e6452de2b182300ccb72e96080d9054f38ba`, tree
`cc41ec7118e019d57f848e84ebbf4a9b4adff5a9`. This receipt proves source
and package identity plus fake-device behavior, **not delivered wheel torque**.
The next physical FFB check requires an attended drive and must not be inferred
from the recorded-playthrough simulation.
