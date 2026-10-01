# Unified F-Zero SNES product candidate

F-Zero SNES and optional BS Deluxe use one native host, existing launcher,
settings set and version. Wheel, FFB, telemetry, recording/replay and experimental
triples are internal features. F-Zero X is a different product. The source
folders in E:/Source are attached development worktrees, not separate products
to merge. Retain the fork name, upstream history and atomic feature commits.

## Install and setup

Extract the complete candidate ZIP to a new folder. Run `Setup.cmd` to open the
existing launcher, choose your own USA ROM, and configure controls there.
`Setup.cmd` only opens that launcher; it does not rewrite settings or profiles.
See [SETUP.md](SETUP.md) for the feature settings and recovery steps.

To update a working install, first close it normally and back up its complete
folder. Verify the candidate in a separate folder before parent-coordinated
deployment. Retain config.ini, fzero-video.ini, keybinds.ini, rom.cfg, saves,
music and user shader folders. Do not extract over a running game. The candidate
does not include an installer that modifies an existing installation.

## Reproducible candidate packaging

The historical `make_release.py` release route is retained. It expects embedded
BS Deluxe data and is not this candidate's distribution route. The unified
candidate excludes the private generated BS module, its embedded payload and
all user data. A stock-only source build must omit FZERO_DELUXE_GEN_DIR and
FZERO_DELUXE_DATA_FILE. No binary from the current BS-equipped installed build
is repackaged here. Public packaging of that module requires a separate review
of distribution rights and build inputs.

`package_unified.py` accepts a prebuilt payload folder and an explicit build
receipt (`dbce.fzero-build`, version 1). The receipt pins source commit/tree,
product version, stock-only mode, dependency identities, PE import closure and
every approved payload file hash. It is build-time consistency evidence, not a
cryptographic attestation of the compiler. Never manufacture a receipt for an
old binary. The packager validates the receipt, x64 PE imports, the pinned
dynamically loaded WheelFfb.dll and notices, then adds this product's docs,
Setup.cmd and machine-readable manifest. ZIP ordering and timestamps are fixed.
It refuses path escapes, symlinks, user settings, private data, shader packs,
missing runtime dependencies, stale hashes and existing outputs. It runs no
payload executable and never scans/copies a whole build folder.

```powershell
python tools/stage_unified.py --build <fresh-stock-only-build> --output <new-stage-directory>
python tools/package_unified.py --payload <stock-only-stage> --receipt <build-receipt.json> --output <new-output-directory>
python tools/package_unified.py --verify <candidate.zip>
python -m unittest discover -s tests -p test_unified_package.py
```

Required payload: FZeroSNESRecomp.exe, WheelFfb.dll, approved build runtime DLLs,
checked-in assets/shaders, the pinned launcher fonts/images, licenses and
dependency/font notices. The staging tool reads CMake's exact dependency roots,
verifies clean gitlinks and the build's source stamp, and pins those assets.
Receipt example and fields are documented in tools/package_unified.py.
The version comes from VERSION; the package channel is candidate, with no new
published release implied. Submodule pins and toolkit file hashes remain
independent of the game version. Experimental actuator DLL overrides must be
reviewed and repinned explicitly; they are not silently treated as v0.13.0.

## Validation boundaries

Physical FFB is opt-in and requires attended acceptance. Replay compares game
simulation and normalized software force requests without wheel actuation.
The current BS replay adapter refuses Deluxe cases until their extra data is
pinned. Three-window diagnostic replay inside Surround does not validate
three physical monitors, scanout, focus or seams. Side world sprites/effects
remain incomplete; keep experimental availability in the product manifest.
No current installed payload, monitor profile or saved tune is changed by this
candidate work.
