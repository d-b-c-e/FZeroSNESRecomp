# DBCE unified preview identity and organization

## One product and setup

Human name: **DBCE F-Zero SNES Unified Preview**. Stable product ID:
`fzero-snes-recomp`. Stock F-Zero SNES and optional private/build-dependent
BS Deluxe share one native host and launcher. Wheel, FFB, telemetry,
recording/replay and experimental triples are internal features, not separate
owned game products. F-Zero X remains a different game target.

Canonical metadata: [game-product.json](../game-product.json).
Canonical product/setup documents: [UNIFIED-PRODUCT](UNIFIED-PRODUCT.md) and
[SETUP](SETUP.md); rights/assets gates: [DISTRIBUTION-AUDIT](DISTRIBUTION-AUDIT.md).
The approved source lane at the canonical target after the pending rename is
[`codex/unified-product-20261001`](https://github.com/d-b-c-e/dbce-mods-fzero-snes/tree/codex/unified-product-20261001).
The one package tool is `tools/package_unified.py` and one `Setup.cmd` opens
`FZeroSNESRecomp.exe --launcher`. The headless companion is the replay runner,
not a second product/install flow. No new installer or game binary is made
by this metadata/organization pass.

## Numeric base and full preview identity

`VERSION=1.8.3` remains numeric for CMake/build receipts and the packager.
`upstreamBaseVersion` explicitly attributes it to upstream 1.8.3; it is NOT a
claim that the DBCE combined product is the upstream 1.8.3 release. Product
metadata supplies the preview name and artifact stem. The packager checks
these plus stable IDs, source branch, documentation links and settings names.

Full manifest label: `DBCE F-Zero SNES Unified Preview 1.8.3+g<SHA12>`.
Filename: `DBCE-FZeroSNES-unified-preview-1.8.3-g<SHA12>-windows-x64.zip`.
The receipt retains the entire source SHA/tree and dependency hashes, not
just the short filename suffix. The archive remains stock-only/ROM-free and
candidate-channel. A new numeric upstream base requires updating VERSION
and the declared base together, not silently treating it as a fork release.

Historical tags, versions, ZIPs and their manifests stay unchanged. Verify a
historical artifact with the verifier from its recorded source revision;
the new metadata contract does not rewrite or relabel it. A later public
preview tag should use an explicitly reviewed fork namespace and migration
plan; no tag, binary release or default-branch change is made here.

`FZeroSNESRecomp.exe`, the headless companion, product ID and config.ini,
fzero-video.ini, keybinds.ini, rom.cfg remain stable. Saves, user shaders,
music, bindings and FFB settings are not moved or rewritten. Human/package
branding is separate from installation compatibility names.

## Reviewed repository-name migration

Canonical target: `d-b-c-e/dbce-mods-fzero-snes`. This source candidate prepares
the current metadata, package verifier and documentation for that rename;
the remote still has its historical name until the separately reviewed
migration is executed. Canonical target links and clone commands become usable
only after that operation. Do not publish this candidate as a current setup
route before coordinating the rename.

Rename the existing public repository, preserving GitHub repository ID
`1380896600`, its fork relationship to
[`mstan/FZeroSNESRecomp`](https://github.com/mstan/FZeroSNESRecomp), upstream
credits and all refs. Fork status does not require a permanent naming exception.
The old owned slug `d-b-c-e/FZeroSNESRecomp` is a historical identifier;
verify its redirect after migration rather than assuming it works.

No replacement repository, upstream rename, source-history rewrite, binary
rename or settings migration is part of this change. Historical artifacts and
their recorded URLs retain their original identity and verifier. The numeric
repository ID is an offline metadata consistency check, not proof of remote
ownership; the migration must verify the live ID, public visibility and fork
parent before and after rename. Main/default promotion requires a separate
complete main-to-unified scope review.

## Observed state and exact remaining organization actions

Read-only inventory on 2026-10-01 listed 75 owned repositories and found one SNES
F-Zero fork, with no separate owned wheel/triple F-Zero repositories. The
eight original F-Zero development folders share one common Git directory;
the isolated review clones/worktrees are validation lanes, not extra owned
mod products. Source histories/feature branches are not folders to merge
blindly. The launcher worktree retains a tracked `recomp/funcs.h` edit.

1. Review this metadata candidate, then promote only its exact source commit
   to the existing unified source lane if separately authorized. No new repo
   or duplicate wheel/triple publication destination is needed.
2. The public default branch remains `main`, while unified source lives on
   the explicit preview branch. At this audit, fork main `1686df46` is an
   ancestor of published preview `69b6aa8` (128 commits ahead, 0 behind). A
   reviewed non-forced main promotion is possible after choosing that
   migration; no reset is needed. Otherwise explicitly retain the preview
   lane. The original main worktree follows upstream/main and is not the
   unified consumer checkout. Coordinate a persistent unified development
   worktree and update portfolio/consumer links; do not reset the upstream
   watcher or modify its checkout as a side effect.
3. Preserve old feature/worktree history and the dirty launcher header. Check
   ancestry and any residual patches with the original owner before retiring
   lanes; a branch tip absent from ancestry does not prove a missing feature
   because prior integration may use cherry-picks. No worktree deletion,
   branch archival or remaining implementation merge is performed here.
   In particular, analog-wheel, force-feedback and telemetry branch tips
   are not ancestry-contained in 69b6aa8 and need residual-patch review before
   retirement. Their current tips match their existing remote branches.
4. Keep the historical BS-equipped release script as a legacy route, not the
   stock preview route. Rights review is required before any BS/public binary
   consolidation; private generated data/configs/replays stay excluded.
5. Agree support intake and finish distribution/attended acceptance gates
   before tester rollout. No Issues/Discussions, device/display, install or
   release settings are changed by this pass.

Software evidence covers one sampled physical layout/replay/CRT preset;
complete side sprites/effects, scanout/comfort, temporal presets and attended
rig/FFB/ROM acceptance remain gated. This organization milestone does not
claim those features complete or the assembled product MIT-only.
