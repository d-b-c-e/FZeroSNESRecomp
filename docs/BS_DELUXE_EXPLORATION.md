# BS F-Zero Deluxe: configurable native mod exploration

Status: accepted for private 1.3.0 release after visible desktop checks. Work
was developed on `feat/bs-fzero-deluxe` in `_wt-fzero-bs-deluxe`. Tracking: central Beads
`beads-8wg.5.3`; framework support: `beads-8wg.2.29` on the nested dependency
branch `feat/fzero-deluxe-native-module`. Framework prerequisites are integrated
before the game pin; the release tag records both dependency revisions.

## Play the checkpoint

For the private release, extract the complete Windows ZIP and run
`FZeroSNESRecomp.exe`. Select your own stock USA ROM, then enable **BS Deluxe**
in **Mods** before Play. The package includes the verified delta and credits;
no import step is needed for this private build. Widescreen and Presentation
FPS are separate optional plugins. No ROM or user save is shipped.

Run `Launch-BS-Deluxe-checkpoint.cmd` from this worktree. The separate
`build-deluxe` directory contains the executable, locally imported data, and
its own launcher configuration. Open **Mods** to enable or disable **BS Deluxe**.
**Widescreen** and **Presentation FPS** remain independent.

The user simplified the scope to **one all-or-nothing package**. There are no
individual feature switches or Custom preset. The complete USA Deluxe delta
(pinned to v1.1 since release 1.4.3; v1.0 through 1.4.2)
is the package's ownership boundary: original and additional courses, eight
machines, alternate courses/cups, records, and Practice ghosts together.
The included upstream readme describes the controls in
`build-deluxe/mods/BS-Deluxe-credits.txt`.

The player still selects the verified stock USA ROM. The importer produces an
indivisible cartridge delta; activation verifies the stock digest and every
resulting byte against the pinned Deluxe digest, then maps a private in-memory
cartridge and selects a separately namespaced compiled module. The ROM file
is never written. A patched ROM is rejected as a player input. This is a full
content-module adaptation, not the independently owned feature conversions used
by the neighboring MMX6 project. The all-or-nothing scope makes that distinction
explicit; no claim of per-feature byte ownership is made.

Deluxe uses `saves/bs-deluxe/save.srm` (32 KiB), while stock uses `saves/save.srm`.
The same subdirectory separation applies under a custom save root. Snapshot
filenames also use a separate prefix. Existing stock saves are not migrated.
The generated native sources, private reference ROM, and imported cartridge
bytes are ignored and must not be committed or bundled into a public release.

## Execution policy and validation limits

Both native modules are linked with distinct function and table symbols;
namespace generation covers definitions, aliases, and direct calls. Launch-time
selection replaces dispatch, inline-argument metadata and WRAM guards together.

**For this checkpoint, Deluxe's main scheduler uses the interpreter floor.**
Its native interrupt helpers and the custom renderer remain active. Initial
race-transition failures appeared sensitive to acceleration around 009EF4,
but the same failure was subsequently reproduced with the interpreter floor.
The confirmed fault was in deferred HDMA rendering, described below; an AOT
miscompile has not been established. Further AOT promotion is deferred. The
stock module keeps its original acceleration policy. This is not a claim that
all Deluxe logic has been validated as native code.

The added vehicle lookup also exposed a plain LoROM unmapped data read at
1E:7F99. The CPU bus previously treated it as an invalid ROM pointer. The scoped
fix preserves the open-bus latch, including word reads across 7FFF/8000; it leaves
RomPtr's pointer guard intact. Hardware behavior reference:
[SNESdev CPU open bus](https://snes.nesdev.org/wiki/Open_bus).

The user-reported car-selection crash was reproduced in the visible desktop
app at frame 789. Deferred HDMA channel 6 read `$0E:2000`; the pointer-based
helper incorrectly treated this hardware-bus address as a ROM pointer and
raised the fatal bus flag despite successful guest execution. Deluxe now uses
guest-address HDMA transfers with RAM, register, cartridge and open-bus reads.
The stock renderer retains its existing HDMA path. A focused test covers
unmapped indirect reads, repetition, termination and wrapping within a bank.
The exact failing input replay now completes 1,200 frames in the visible
desktop app, with a captured frame showing the race start and no fatal error.

The requested visible pairing checks also reached and ran Blue Thunder on
Forest I (BS content) and Blue Falcon on Mute City I (original content), with
Deluxe enabled for both. These bounded acceleration replays test loading and
race execution, not full-race completion or handling quality.

Those checks exposed invisible league/class text despite valid BG3 graphics.
The shared PPU scanout always ORed its two window masks, ignoring WBGLOG and
WOBJLOG. Deluxe uses XOR overlap to reveal the menu. The PPU now honors OR,
AND, XOR and XNOR when both windows are enabled; the focused PPU regression
checks actual rendered BG3 pixels for every operation. The custom widescreen
renderer already honored these operations.
After rebuilding, both visible pairings completed 1,600 frames without a
runtime error. Captures verify the selected machines, Knight/BS-1 league menus,
Mute City I/Forest I titles and moving vehicles in each race. The menu text is
visible again. Final user validation remains pending.

Completed checks:

- Independent IPS/BPS agreement and CRC checks; synthetic overlap, malformed
  stream and CRC rejection tests.
- Five CTest suites pass, including the new HDMA regression test and the
  independent content toggle.
- Framework dispatch contract test extended for alternate tables, restoring
  stock, mirrored open bus and the unmapped-to-ROM word boundary.
- Stock-disabled 600-frame boot; Deluxe 1,800-frame attract and wide race routes.
- Captured both machine-selection groups and inspected race rendering.
- Desktop 240-FPS-target smoke run; actual target attainment is hardware limited.
- Desktop save test wrote exactly 32,768 bytes under the Deluxe directory and
  left a stock-save sentinel unchanged. Corrupted mod data and patched-ROM player
  inputs were rejected. The stock ROM digest remains unchanged.

**Remaining coverage: user playtest.** Check original and BS leagues, all eight
machines, alternate layouts/cups, ghost recording/playback and persistence, and
widescreen/FPS combinations. Full-cup coverage, all new stages, and ghost replay
have not been validated. Further automated checks should follow specific
failures rather than delaying this hands-on checkpoint.

## Verified archive evidence

### Current pin: USA v1.1 (release 1.4.3 onward)

Input: user-supplied `E:/Downloads/BS F-Zero Deluxe.zip` (upstream v1.1,
readme dated March 29, 2025; version history entry April 1, 2025). The archive
renames the patches to `patches/bs-deluxe-v1.1-<region>.{bps,ips}`; the tools
now locate the single USA BPS/IPS pair by name pattern and parse the version
from the readme title, so both layouts import. Upstream v1.1 changes per its
readme: official BS F-Zero Grand Prix 2 Week 1 data (Forest course graphics,
Forest I/II track and path, BS-1 League race parameters, adjusted Forest III),
fixes for a CPU crash when loading courses and garbled records graphics on
first-generation consoles, and league-aware opponent speeds and Exploding
Bumper spawn rates.

USA IPS and BPS produce the same 1 MiB image; BPS CRCs verify. Against the
v1.0 image, v1.1 changes 281,720 bytes in 14,411 ranges: 113 bytes in bank 00,
14 in bank 03, and the rest in the appended banks 14-1F. Against stock the
changed original-region byte count is 58,222 (v1.0: 58,209); the 00:C339
renderer hook still differs from stock and the other five hooks still match.
The Deluxe native module was regenerated from the v1.1 oracle (9 roots,
37 exact AOT variants, 1 LLE variant, same shape as v1.0); the importer
produced 47,168 guarded delta records (791,073 bytes).

| Artifact | SHA-256 |
|---|---|
| Archive (v1.1) | `86b39e75186b68f8ff2a848cd7d42b66366720aad7d1fdabea2c3735ba4f3cda` |
| USA BPS (v1.1) | `3eb3ead8b5452d95f35c8c477a101984fdc49653e6da7ead2619d160416f1478` |
| USA IPS (v1.1, tracked as `patches/bs-deluxe-usa.ips`) | `2f0217a96209b5d9fd3f0f2ed348086fdc5002a478a9557d3fd522ff5fddd2f6` |
| Private patched reference (v1.1) | `552159a19955e88a8337f7c473ccc53e5dcef15b87daab8e89e1894694542fe6` |

The pin lives in three places that must move together: `DELUXE_SHA256` /
`DELUXE_VERSION` in `tools/import_bs_deluxe.py`, `target_hash` in
`src/fzero_deluxe.c`, and the regenerated module in `captures/bs-deluxe/gen`.
A v1.0 payload is rejected by the v1.1 build and vice versa.

### Previous pin: USA v1.0 (releases 1.3.0 through 1.4.2)

Input: user-supplied `E:/Downloads/bs_f-zero_deluxe_v1.0.zip`.
The included readme identifies version 1.0, February 10, 2024, and credits
GuyPerfect, PowerPanda, Porthor, Catador and footage contributor kukun kun.
Retain the supplied credits in the eventual local import flow.

The readme describes all 25 courses in Practice, BS-1 and BS-2 GP leagues,
eight vehicles, alternate course versions and mixed leagues selected with
L+R, and a single saved Practice ghost. Big Blue II and Silence II variants
have separate records. Some alternate layouts are cosmetic. The vehicle
selection also affects rival and drone behavior; new vehicles are not solely
a graphics replacement. These are documented upstream behaviors, not yet
verified in this recomp.

USA IPS and BPS patches independently produce exactly the same reference image.
The BPS source, target and patch CRCs all verify.

| Artifact | SHA-256 |
|---|---|
| Archive | `36935d50a036a1b269940051db4007ad25d1e41c91449390039b68e395132efb` |
| Stock USA ROM | `bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2` |
| USA BPS | `8ee4baff76abd03e5e33dbf392b7aaecdcb5eaa885c445f7e200911002f301f3` |
| USA IPS | `e80ff101b09c347e399394ae0624c18186cd391a5ebf0a2f5c64741470f02582` |
| Private patched reference | `77bb37bcdedd3e17321727d5ed6a14792aa7a45ac04e9040b16bf564bd6dea24` |

The cartridge header changes from 512 KiB ROM / 2 KiB SRAM to 1 MiB ROM /
32 KiB SRAM. The original-size region contains 58,209 changed bytes in 437
contiguous ranges. Changes occur in banks 00, 01, 02, 03, 06 and 07; appended
bytes still require code/data/padding classification.

A conservative interval comparison against the released native program manifest
finds 69 potentially affected AOT variants. This is an audit lead, not a complete
call graph or feature ownership map. The first 16 bytes at the widescreen
projection hook 00:DCC6 match; the shadow routine at 00:C339 changes. Neither
observation proves renderer compatibility across the entire routine.

## Regenerate and build

After staging the verified stock `fzero.sfc`, initialize the pinned dependencies
and generate stock sources with `tools/regen.sh`. Build the native analyzer with
`snesrecomp/tools/build_native_analyzer.py` (or set `SNESRECOMP_NATIVE_ANALYZER`
to a compatible pinned executable). Then run:

```powershell
python tools/regen_bs_deluxe.py --archive "E:/Downloads/BS F-Zero Deluxe.zip"
cmake -S . -B build-deluxe -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DFZERO_DELUXE_GEN_DIR=$PWD/captures/bs-deluxe/gen"
cmake --build build-deluxe
```

On Windows, invoke native CMake, Ninja and compiler executables directly when
PATH contains MSYS shims. The shared framework changes currently live in this
worktree's dependency branch; they are not available in the released pin yet.
The importer preserves the original credits and records input and output hashes
beside the private data. No upstream feature-level source code is assumed.

## Reproduce the audit

From this worktree, using a Python 3 executable:

```powershell
python tools/inspect_bs_deluxe.py `
  --archive "E:/Downloads/BS F-Zero Deluxe.zip" `
  --stock ../_wt-fzero-adaptive-renderer/fzero.sfc `
  --manifest ../_wt-fzero-adaptive-renderer/src/gen/program_manifest.json `
  --out captures/bs-deluxe/audit.json
```

Optional `--oracle captures/bs-deluxe/oracle.sfc` creates a private reference
with exclusive creation; it refuses to overwrite an existing file. The generated
report contains offsets and hashes, not ROM payload. `captures/` is ignored.
Source references: the supplied archive's `readme.txt` and USA patches,
`../psxrecomp/MegaManX6Recomp/docs/MMX6_TWEAKS_MODS.md` and
`docs/PSXMOD_CONVERSION_GUIDE.md` in that repository, and this worktree's
`snesrecomp/runner/src/mod_runtime.h`.

## Embedded payload

The imported payload ships inside the executable rather than beside it: a
download can never be missing it, and the game can never fail to launch over
it. `tools/embed_payload.py` turns `bs-deluxe.dat` into a C source that both
hosts link, driven from CMake whenever `FZERO_DELUXE_GEN_DIR` is set;
`FZERO_DELUXE_DATA_FILE` overrides where that payload is read from and defaults
to the `mods/` directory `tools/regen_bs_deluxe.py` writes beside the generated
sources.

`FzeroDeluxePrepare` verifies the embedded bytes exactly as it verified a file
- magic, declared sizes, the stock digest they were built against, ordered
non-overlapping records, and the digest of the patched cartridge - and applies
them from memory. A file at `mods/bs-deluxe.dat`, or the path in
`FZERO_DELUXE_DATA`, is still tried first so an importer run can be checked
without rebuilding; it is a development input, and the embedded copy is the
fallback that always exists.

Nothing about Deluxe is fatal any more. If preparation fails for any reason the
host logs `[bs-deluxe] ... starting stock`, clears the setting for that session
only, and runs the stock cartridge; the user's `fzero-video.ini` is left alone
so fixing the build or removing the override brings Deluxe back. `mods/`
continues to ship `BS-Deluxe-credits.txt` and `bs-deluxe-import.json` for
credits and provenance, and `tools/make_release.py` refuses to package a build
whose executable does not contain the payload magic and the expected target
digest.
