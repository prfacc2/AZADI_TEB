#!/bin/bash
# Capture a single screen with given env vars. Usage: ./capture.sh <outname> KEY=val KEY=val ...
set -e
cd "$(dirname "$0")"
export WINEDEBUG=-all WINEPREFIX=$HOME/.wine DISPLAY=:99
OUT="shots/shot_$1.png"; shift
# parse env assignments
ENVS=()
for kv in "$@"; do ENVS+=("$kv"); done
if ! pgrep -f "Xvfb :99" >/dev/null; then
  Xvfb :99 -screen 0 1600x900x24 >/tmp/xvfb.log 2>&1 &
  sleep 3
fi
env "${ENVS[@]}" wine build/AzadiTeb_dbg.exe >/dev/null 2>&1 &
WPID=$!
sleep 8
import -window root "$OUT" 2>/dev/null || true
kill $WPID 2>/dev/null || true
wineserver -k 2>/dev/null || true
sleep 1
[ -f "$OUT" ] && echo "Saved $OUT ($(stat -c%s "$OUT") bytes)" || echo "NO SHOT $OUT"
