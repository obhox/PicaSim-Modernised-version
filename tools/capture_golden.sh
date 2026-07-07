#!/usr/bin/env bash
#
# capture_golden.sh — capture a golden-image baseline screenshot.
#
# Phase 0 modernization harness. The PicaSim binary supports:
#   --screenshot-after N          capture the back buffer at frame N, then quit
#   --screenshot-file <path>      write the PNG to <path> (else user data/Screenshots)
#   F12 (in-app)                  capture the current frame on demand (menus or flight)
#
# The automated flag captures whatever scene is on screen at frame N. Because
# PicaSim boots into the start menu, use --freefly-style startup (set
# mFreeFlyOnStartup + last-used aircraft/environment in settings.xml) to land
# directly in a flight scene, or press F12 while navigating manually.
#
# Usage:
#   tools/capture_golden.sh <label> [frame]
# Example:
#   tools/capture_golden.sh flatland-startmenu 90
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${PICASIM_BIN:-$ROOT/build/macos-arm64/Debug/PicaSim}"
OUTDIR="${PICASIM_GOLDEN_DIR:-$ROOT/ReferenceMaterial/Golden}"
LABEL="${1:?usage: capture_golden.sh <label> [frame]}"
FRAME="${2:-90}"

mkdir -p "$OUTDIR"
OUT="$OUTDIR/$LABEL.png"

echo "Capturing '$LABEL' at frame $FRAME -> $OUT"
( cd "$ROOT/data" && "$BIN" --screenshot-after "$FRAME" --screenshot-file "$OUT" )
echo "Done: $OUT"
