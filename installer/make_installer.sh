#!/usr/bin/env bash
# Builds the Windows installer: manual + licences into dist/payload-lumen, then NSIS.
#   ./build.sh windows && installer/make_installer.sh   -> dist/Lumen-1.5-Windows-Setup.exe
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
P="$ROOT/dist/payload-lumen"
mkdir -p "$P"
cp "$ROOT/LICENSE" "$P/LICENSE.txt"
cp "$ROOT/THIRD-PARTY.txt" "$P/THIRD-PARTY.txt"
python3 "$ROOT/docs/make_lumen_manual.py" "$P/Lumen-Manual.pdf" "$ROOT/docs/lumen_screenshot.png" "$ROOT/docs/lumen_edit.png" "$ROOT/docs/lumen_timeline.png"
cd "$ROOT/installer" && makensis -V2 lumen.nsi
ls -la "$ROOT"/dist/Lumen-*-Windows-Setup.exe
