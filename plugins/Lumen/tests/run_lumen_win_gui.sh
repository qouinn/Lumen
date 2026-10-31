#!/usr/bin/env bash
# Windows build of Lumen under Wine: pick a category + sound, turn a macro, Play, then drag the render out into a drop target.
set -u
S="${1:-1}"; OUT="${2:-.}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DISP=":$((60 + RANDOM % 20))"
Xvfb $DISP -screen 0 ${SCREEN:-1700x1300}x24 >/dev/null 2>&1 & XPID=$!
sleep 1
export DISPLAY=$DISP WINEDEBUG=-all
rm -rf "$HOME/.wine/drive_c/users/${USER:-root}/Documents/Grainfield Renders" 2>/dev/null
REPORT="$OUT/lumen_win_report_$S.txt"; rm -f "$REPORT"
GFH_HOSTSCALE=${GFH_HOSTSCALE-$S} GFH_PARAMS="Macro Set,Macro 1,Play,Timeline On,Timeline Sections,Section 2 Sound,Play From" wine "$ROOT/build/test/clap_gui_win.exe" "${GF_PLUGIN:-$ROOT/bin/windows/Lumen.clap}" 34 "$(winepath -w "$REPORT")" &
WPID=$!
for i in $(seq 1 60); do [ -s "$REPORT" ] && break; sleep 0.5; done
sleep 2
# the editor's real size decides where the clicks go (it may have been fitted to the screen)
GOT=$(grep -o 'size=[0-9]*x[0-9]*' "$REPORT" | head -1 | cut -d= -f2)
[ -n "$GOT" ] && S=$(python3 -c "w,h=map(int,'$GOT'.split('x')); print(min(w/1100,h/760))")
echo "editor size $GOT -> click scale $S"
c() { python3 -c "print(int(round($1*$S)), int(round($2*$S)))"; }
xdotool mousemove $(c 111 381) click 1; sleep 0.6          # PADS & TEXTURES (row 4: ALL, NEW, GRANULAR, SHIMMER, PADS)
xdotool mousemove $(c 580 290) click 1; sleep 0.6          # 2nd sound
xdotool mousemove $(c 815 372) mousedown 1; sleep 0.3; xdotool mousemove $(c 815 330); sleep 0.3; xdotool mousemove $(c 815 300); sleep 0.3; xdotool mouseup 1; sleep 0.5
xdotool mousemove $(c 1009 211) click 1; sleep 0.8        # SAVE AS OWN PRESET (the macro was turned)
xdotool type --delay 80 'Wine Sound'; sleep 0.4; xdotool key Return; sleep 1.0
import -window root -crop $(python3 -c "print(f'{int(1100*$S)}x{int(760*$S)}+0+0')") "$OUT/lumen_win_saved_$S.png"
xdotool mousemove $(c 403 211) click 1; sleep 0.6         # TIMELINE page
xdotool mousemove $(c 550 330) click --repeat 2 --delay 80 1; sleep 0.8   # add section 2 in the middle
xdotool mousemove $(c 171 496) click 1; sleep 0.5          # SPACE
xdotool mousemove $(c 348 549) click 1; sleep 3            # play button on Cathedral: plays from section 2
import -window root -crop $(python3 -c "print(f'{int(1100*$S)}x{int(760*$S)}+0+0')") "$OUT/lumen_win_${TAG:-$S}.png"
xdotool mousemove $(c 862 31) click 1; sleep 0.8
xdotool mousemove $(c 951 31) mousedown 1; sleep 4
xdotool mousemove $(c 960 40); sleep 0.4
xdotool mousemove 1100 900; sleep 0.4
xdotool mousemove 1250 980; sleep 0.4
xdotool mousemove 1260 990; sleep 0.6
xdotool mouseup 1; sleep 1.5
wait $WPID
kill $XPID 2>/dev/null
cat "$REPORT"
WD="$HOME/.wine/drive_c/users/${USER:-root}/Documents/Lumen/Own Presets.txt"
if grep -q "|Wine Sound|" "$WD" 2>/dev/null; then echo "own preset saved: yes"; else echo "own preset saved: NO"; fi
