#!/usr/bin/env bash
# test_appimage_layout.sh <AppDir>
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/AppDir" >&2
  exit 2
fi

APPDIR="$(CDPATH= cd -- "$1" && pwd)"
test -x "$APPDIR/AppRun" || { echo "missing executable AppRun" >&2; exit 1; }
test -x "$APPDIR/usr/bin/FZeroSNESRecomp" || { echo "missing FZeroSNESRecomp ELF" >&2; exit 1; }
test -d "$APPDIR/usr/bin/assets" || { echo "missing launcher assets" >&2; exit 1; }
test -f "$APPDIR/usr/bin/assets/shaders/crt-soft.glslp" || { echo "missing CRT shader" >&2; exit 1; }

if [ -d "$APPDIR/usr/bin/mods" ]; then
  test -f "$APPDIR/usr/bin/mods/bs-deluxe.dat" || { echo "missing BS Deluxe payload" >&2; exit 1; }
  test -f "$APPDIR/usr/bin/mods/BS-Deluxe-credits.txt" || { echo "missing BS Deluxe credits" >&2; exit 1; }
fi
if [ -d "$APPDIR/usr/bin/patches" ]; then
  test -f "$APPDIR/usr/bin/patches/bs-deluxe-usa.ips" || { echo "missing BS Deluxe IPS patch" >&2; exit 1; }
fi

TMP="$(mktemp -d)"
trap 'chmod -R u+w "$APPDIR" "$TMP" 2>/dev/null || true; rm -rf "$TMP"' EXIT HUP INT TERM
chmod -R a-w "$APPDIR"

STATE="$TMP/state"
mkdir -p "$STATE"
: > "$STATE/pretend.sfc"
( cd "$STATE" && APPIMAGE="$STATE/FZeroSNESRecomp.AppImage" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout 20 "$APPDIR/AppRun" >/dev/null 2>&1 ) || true

test -f "$STATE/rom.cfg" || { echo "adjacent ROM did not seed rom.cfg" >&2; exit 1; }
test "$(head -n1 "$STATE/rom.cfg")" = "$STATE/pretend.sfc" || {
  echo "rom.cfg points at the wrong ROM" >&2
  exit 1
}
if [ -d "$APPDIR/usr/bin/patches" ]; then
  test -f "$STATE/patches/bs-deluxe-usa.ips" || {
    echo "AppRun did not expose the BS Deluxe IPS patch beside the AppImage" >&2
    exit 1
  }
fi

for leak in rom.cfg keybinds.ini fzero-video.ini saves last_run_report.json; do
  found="$(find "$APPDIR" -name "$leak" -print -quit)"
  [ -z "$found" ] || { echo "state leaked into AppDir: $found" >&2; exit 1; }
done

echo "AppImage layout test passed"
