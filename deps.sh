#!/usr/bin/env bash
# Downloads the two libraries Lumen builds on, at the exact versions it was tested with.
#   DPF (DISTRHO Plugin Framework) -> dpf/      dr_libs (WAV/FLAC/MP3 loading) -> plugins/Shared/ext/drlibs/
set -euo pipefail
cd "$(dirname "$0")"
fetch() { # url dir commit
  if [ -e "$2/.git" ] || [ -f "$2/README.md" ]; then echo "  $2 already there"; return; fi
  git clone --quiet "$1" "$2"
  git -C "$2" checkout --quiet "$3"
  git -C "$2" submodule update --init --recursive --quiet
  echo "  $2 @ ${3:0:7}"
}
fetch https://github.com/DISTRHO/DPF.git dpf 4238e1c7f0351bbe488d79f0899c540543ac7583
fetch https://github.com/mackron/dr_libs.git plugins/Shared/ext/drlibs b10929d6a10c26ce0d22e5e8b2299714f08474eb
