# Stock-only preview distribution audit

The bounded packaging/export changes based on reviewed source `5432eb1`
passed independent review and were published as source `69b6aa8` on the
existing unified preview branch. Both GitHub-generated source archive
formats at that exact commit were inspected: cover/screenshots/patches are
excluded and 158 regular files match the canonical commit export. The later
identity/organization pass is a separate review candidate. No unified binary
release/tag/installation or tester rollout is implied by source publication.
Removing identified assets is a packaging boundary, not legal clearance.

## Identified packaging defect and candidate boundary

The original stage/verify contract required `assets/img/boxart.tga`. Its
documented origin is Nintendo's North American F-Zero cover via Libretro;
`assets/README.md` explicitly excludes it from the source-code license.
The ZIP also copied six SNES/BS gameplay screenshots as documentation extras.
The candidate omits both sets and rejects their paths in received payloads.
ROMs, BS/generated modules, patches, recordings, saves and private settings
are excluded by the explicit payload/package file sets. No source or user
asset is deleted or overwritten, and the installed launcher is unaffected.

Pinned recomp-ui `0c15219a79d39c41981425c69898a0f8bfcaf902`:
`src/common/launcher_gl.c` initializes texture `{0,0,0}` and returns it on
missing-image decode before any GL call. `launcher_imgui.cpp` loads the
default cover path and `hero_boxart_centered()` draws its vector placeholder
when texture ID/size are absent. No fatal check depends on this cover.
The candidate build copy is conditional so source-archive builds can omit
the file. Full checkouts retain the previous optional copy. Packaging always
omits it, even when it exists in a build. Loader unit tests do not start SDL,
create a window/context, enumerate devices, run the game or actuate FFB.
This is source/unit assurance, not a visually verified launcher startup.

## Mixed code and asset terms

| Component or asset | Pinned evidence | Distribution status / remaining gate |
|---|---|---|
| Top-level game code | Repository `LICENSE`: MIT, Matthew Stanley | Preserve notice; applies to this component, not the whole assembled product |
| snesrecomp engine `59d2966fa71d0346f92c43753e09b1ba7a1956f5` | Engine `LICENSE`: PolyForm Noncommercial 1.0.0 plus appended profit-derived-use restriction and commercial inquiry contact | Exact full license retained; review intended community/commercial use against BOTH sets of terms; do not call the product MIT-only |
| recomp-ui code at above pin | Dependency `LICENSE`: MIT | Exact license retained; does not independently resolve origin/rights of every image |
| Dear ImGui | Pinned dependency `src/third_party/imgui/LICENSE.txt` | Preserve MIT notice |
| WheelFfb toolkit | Retained toolkit pin and `lib/toolkit/LICENSE.txt`: MIT, d-b-c-e | Exact hash/license retained; physical-device acceptance is separate |
| SDL runtime / additional DLLs | Exact build dependencies and import closure; staging requires notice for every extra runtime | Review actual final runtime notices/pins; synthetic tests do not certify compiler provenance |
| Nintendo cover | `assets/README.md`, retrieved September7,2026 | Excluded from binary ZIP and future candidate source exports; still present in repository/history/local builds |
| Gameplay screenshots | Six tracked `docs/screenshots/*.png`, showing stock/BS game visuals | Excluded from binary ZIP and future candidate source exports; screenshot capture does not establish redistribution rights |
| Country flags `flags.png` | Pinned `assets/common/img/NOTICE.md`: rendered from Noto Color Emoji, Noto Project Authors, OFL1.1 | Preserve upstream notice plus complete pinned OFL; confirm exact upstream font provenance/notice sufficiency before rollout |
| Lato Latin Bold/Regular | Font embedded notices plus complete pinned Lato OFL | Preserve embedded notices, full OFL and Reserved Font Name conditions |
| Noto Sans Symbols2 | Embedded notices plus complete pinned Noto OFL | Preserve embedded notices/full OFL |
| OpenMoji font | Embedded notices and pinned fonts NOTICE: CC BY-SA4.0, OpenMoji Project | Preserve attribution/terms link; review share-alike/license obligations for redistributed font; do not assume MIT |
| `brand_mark.tga`, `pad.tga`, four verdict images | Bytes match exact recomp-ui pin; general MIT license; image NOTICE documents flags only | Image-specific authorship/source/rights not established by inspected notices. UNRESOLVED: request provenance or replace under separately reviewed scope |
| Four shader presets/passes | Source headers identify original Mega Man X Recomp presets as public domain / CC0-1.0 | Declared dedication retained in source; verify author/provenance and sufficient notice material before distribution; not blanket clearance |

Staging now preserves pinned launcher image/font NOTICE files and the full
OFL text for flag-font imagery in addition to existing embedded font notices.
It does not invent missing licenses for the other images.

## BS patch and automatic source archives

Tracked `patches/bs-deluxe-usa.ips` is 493098bytes, SHA256
`2f0217a96209b5d9fd3f0f2ed348086fdc5002a478a9557d3fd522ff5fddd2f6`.
It entered this source history in commit `56c99f21edc342bd644dbbdc877d8d6ba69b189c`
("Bundle BS F-Zero Deluxe v1.1 as the default mod").
`BS_DELUXE_EXPLORATION.md` records a user-supplied upstream v1.1 archive,
readme dated March29,2025, and official BS Grand Prix2-derived content.
The historical README asserts inclusion with the authors' permission; that
is existing permission evidence, but its scope and terms are not supplied.
No patch-specific redistribution license was located in the tracked source.
Origin evidence and hashes are NOT permission. This audit does not conclude
that an IPS file is infringing; inclusion remains unresolved pending patch
author/readme terms and review of embedded content. The candidate excludes
the entire patches directory from future source archive exports, preserving
tracked/local files and history. Private importer workflows still require
user-supplied verified assets; no ROM/BS data is bundled by the ZIP tooling.

GitHub generates branch/tag/commit ZIP/tar snapshots with `git archive`:
[GitHub archive documentation](https://docs.github.com/en/repositories/working-with-files/using-files/downloading-source-code-archives).
Git's [`export-ignore` documentation](https://git-scm.com/docs/git-archive)
defines the exclusion mechanism. Candidate `.gitattributes` excludes the
cover, screenshots directory and patches directory. Local ZIP and tar
exports must be checked at the exact candidate commit. Before a future
release, also download BOTH GitHub auto-generated formats at its exact
commit and inspect their manifests; hosting verification passed exact
`69b6aa8` and must be repeated for any later release commit. Old references/clones/history still contain
these assets. The current published source is NOT asset-free. No history
rewrite, deletion of owner assets or retroactive archive claim is made.

## Distinct preview identity and rollout gates

Keep upstream `VERSION=1.8.3` as a base version, not a new upstream release.
Proposed manifest identity: `DBCE F-Zero SNES Unified Preview
1.8.3+g<12-character-source-SHA>`; candidate ZIP filename starts
`DBCE-FZeroSNES-unified-preview-1.8.3-g<SHA>-windows-x64.zip`.
The candidate tooling records/enforces this identity, avoiding the existing
upstream 1.8.3 release identity. Any eventual tag should use a distinct fork
preview namespace and immutable source/package hashes, after review. No tag
or release is created by this work.

Remaining gates: independent packaging/loader/export review, including the
actual clean stock build/package receipt; exact-hosted source archive audit if published;
uncertain UI imagery/provenance, OpenMoji/flag/shader notice review; intended
use review for PolyForm plus added engine restriction; patch terms if ever
distributed; tester-facing limitations and attended physical acceptance.
Issues and Discussions are currently reported disabled on the fork. A clear
support intake should be agreed before tester rollout; no settings changes
are authorized or made by this candidate. ROM legality and permissions are
not certified by package tests.
