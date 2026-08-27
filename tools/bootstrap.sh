#!/usr/bin/env bash
# Initialize and verify every dependency needed by this checkout.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! git -C "$ROOT" rev-parse --show-toplevel >/dev/null 2>&1; then
  echo "bootstrap.sh: $ROOT is not a Git checkout" >&2
  exit 1
fi

echo "=== Synchronizing submodule URLs ==="
git -C "$ROOT" submodule sync --recursive

echo "=== Initializing pinned submodules ==="
git -C "$ROOT" submodule update --init --recursive

for module in snesrecomp recomp-ui; do
  expected="$(git -C "$ROOT" ls-files --stage -- "$module" | awk '$1 == "160000" { print $2 }')"
  actual="$(git -C "$ROOT/$module" rev-parse HEAD 2>/dev/null || true)"
  if [ -z "$expected" ] || [ "$actual" != "$expected" ]; then
    echo "bootstrap.sh: $module is at ${actual:-<missing>}, expected ${expected:-<missing gitlink>}" >&2
    exit 1
  fi
done

if [ ! -f "$ROOT/snesrecomp/runner/runner.cmake" ] ||
   [ ! -f "$ROOT/snesrecomp/tools/v2_emit.py" ] ||
   [ ! -f "$ROOT/recomp-ui/recomp_ui.cmake" ]; then
  echo "bootstrap.sh: a pinned dependency checkout is incomplete" >&2
  exit 1
fi

printf '\nReady: snesrecomp and recomp-ui are initialized.\n'
printf 'Next: stage your legally obtained ROM as fzero.sfc and run bash tools/regen.sh\n'
