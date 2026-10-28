#!/usr/bin/env bash
# Build script for Lumen
#   ./build.sh windows   -> Windows x64 VST3 + CLAP (cross-compiled with mingw-w64)  -> bin/windows
#   ./build.sh linux     -> native Linux VST3 + CLAP                                  -> bin/linux
set -euo pipefail

TARGET="${1:-windows}"
PLUGIN="${PLUGIN:-Lumen}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
DPF="$ROOT/dpf"
SRC="$ROOT/plugins/$PLUGIN"
SHARED="$ROOT/plugins/Shared"              # AudioFile, Export, fonts, SampleData
OUT="$ROOT/bin/$TARGET"
OBJ="$ROOT/build/$TARGET"
POBJ="$OBJ/plugin-$PLUGIN"
NAME="$PLUGIN"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"

case "$PLUGIN" in
  Lumen)      PLUGIN_SRCS="LumenPlugin.cpp LumenUI.cpp" ;;
  *) echo "unknown plugin $PLUGIN"; exit 1 ;;
esac

[ -f "$DPF/distrho/DistrhoPlugin.hpp" ] && [ -f "$SHARED/ext/drlibs/dr_wav.h" ] || bash "$ROOT/deps.sh"   # first build: fetch DPF + dr_libs
mkdir -p "$OBJ/dgl" "$POBJ" "$OUT"

COMMON="-O3 -ffast-math -fdata-sections -ffunction-sections -DNDEBUG -fvisibility=hidden -Wall -Wno-unused-parameter"
case "$TARGET" in
  windows)
    CC=x86_64-w64-mingw32-gcc; CXX=x86_64-w64-mingw32-g++
    PLAT="-mstackrealign -mtune=generic -msse -msse2 -mfpmath=sse -D__STDC_FORMAT_MACROS=1 -D__USE_MINGW_ANSI_STDIO=1"
    SYS_DGL=""
    LINK="-shared -static -static-libgcc -static-libstdc++ -Wl,-O1,--as-needed,--gc-sections -Wl,--strip-all -Wl,--no-undefined"
    LIBS="-lopengl32 -lcomdlg32 -ldwmapi -lgdi32 -lole32 -lshell32 -luuid -lmfuuid -lm"
    VST3_BIN="$OUT/$NAME.vst3/Contents/x86_64-win/$NAME.vst3"
    CLAP_BIN="$OUT/$NAME.clap"
    VST3_SYMS="$DPF/utils/symbols/vst3.def"; CLAP_SYMS="$DPF/utils/symbols/clap.def"
    ;;
  linux)
    CC=gcc; CXX=g++
    PLAT="-fPIC -DPIC -mtune=generic -msse -msse2 -mfpmath=sse"
    SYS_DGL="-DHAVE_X11 -DHAVE_XCURSOR -DHAVE_XEXT -DHAVE_XSYNC -DHAVE_XRANDR"
    LINK="-shared -Wl,-O1,--as-needed,--gc-sections -Wl,--strip-all -Wl,--no-undefined"
    LIBS="-lGL -lX11 -lXcursor -lXext -lXrandr -ldl -lpthread -lm"
    VST3_BIN="$OUT/$NAME.vst3/Contents/x86_64-linux/$NAME.so"
    CLAP_BIN="$OUT/$NAME.clap"
    VST3_SYMS="-Wl,--version-script=$DPF/utils/symbols/vst3.version"; CLAP_SYMS="-Wl,--version-script=$DPF/utils/symbols/clap.version"
    ;;
  *) echo "unknown target $TARGET"; exit 1;;
esac

INC="-I$DPF/distrho -I$DPF/dgl -I$DPF/dgl/src -I$DPF/dgl/src/pugl-upstream/include"
DGLBASE="$COMMON $PLAT $SYS_DGL -std=gnu++17 -DHAVE_OPENGL -DDGL_USE_FILE_BROWSER -DDONT_SET_USING_DGL_NAMESPACE $INC"
# plugin dir first so its DistrhoPluginInfo.h wins
PLUGFLAGS="$COMMON $PLAT $SYS_DGL -std=gnu++17 -DHAVE_OPENGL -DDGL_OPENGL -DHAVE_DGL -DDGL_USE_FILE_BROWSER -I$SRC -I$SHARED $INC"

# newest header among the plugin + shared sources: any change rebuilds the plugin objects
newest_header() { find "$SRC" "$SHARED" -maxdepth 2 -name '*.h*' -newer "$1" 2>/dev/null | head -1; }

jobs_wait() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 0.2; done; }
compile() { # out src flags...
  local out="$1" src="$2"; shift 2
  if [ ! -f "$out" ] || [ "$src" -nt "$out" ] || { [ "${CHECK_HEADERS:-0}" = 1 ] && [ -n "$(newest_header "$out")" ]; }; then
    echo "  CXX $(basename "$src")"
    "$CXX" "$src" "$@" -c -o "$out" &
    jobs_wait
  fi
}

echo "== DGL ($TARGET)"
for f in Application ApplicationPrivateData Color EventHandlers Geometry ImageBase ImageBaseWidgets Layout Resources \
         SubWidget SubWidgetPrivateData TopLevelWidget TopLevelWidgetPrivateData Widget WidgetPrivateData Window WindowPrivateData; do
  compile "$OBJ/dgl/$f.o" "$DPF/dgl/src/$f.cpp" $DGLBASE
done
for f in OpenGL OpenGL2 NanoVG; do
  compile "$OBJ/dgl/$f.gl.o" "$DPF/dgl/src/$f.cpp" $DGLBASE -DDGL_OPENGL
done
compile "$OBJ/dgl/pugl.gl.o" "$DPF/dgl/src/pugl.cpp" $DGLBASE -DDGL_OPENGL -Wno-extra -Wmissing-field-initializers
wait

echo "== $PLUGIN"
export CHECK_HEADERS=1
BASE_OBJS=""
for s in $PLUGIN_SRCS; do
  compile "$POBJ/${s%.cpp}.o" "$SRC/$s" $PLUGFLAGS
  BASE_OBJS="$BASE_OBJS $POBJ/${s%.cpp}.o"
done
compile "$POBJ/AudioFile.o" "$SHARED/AudioFile.cpp" $COMMON $PLAT -std=gnu++17 -I$SHARED -Wno-unused-function
compile "$POBJ/Export.o"    "$SHARED/Export.cpp"    $COMMON $PLAT -std=gnu++17 -I$SHARED
BASE_OBJS="$BASE_OBJS $POBJ/AudioFile.o $POBJ/Export.o"
for fmt in VST3 CLAP; do
  compile "$POBJ/PluginMain_$fmt.o" "$DPF/distrho/DistrhoPluginMain.cpp" $PLUGFLAGS -DDISTRHO_PLUGIN_TARGET_$fmt
  compile "$POBJ/UIMain_$fmt.o"     "$DPF/distrho/DistrhoUIMain.cpp"     $PLUGFLAGS -DDISTRHO_PLUGIN_TARGET_$fmt
done
wait
unset CHECK_HEADERS

DGL_OBJS="$(ls "$OBJ"/dgl/*.o)"
echo "== Link"
mkdir -p "$(dirname "$VST3_BIN")" "$(dirname "$CLAP_BIN")"
$CXX $BASE_OBJS "$POBJ/PluginMain_VST3.o" "$POBJ/UIMain_VST3.o" $DGL_OBJS $LINK $VST3_SYMS $LIBS -o "$VST3_BIN"
echo "  -> $VST3_BIN"
$CXX $BASE_OBJS "$POBJ/PluginMain_CLAP.o" "$POBJ/UIMain_CLAP.o" $DGL_OBJS $LINK $CLAP_SYMS $LIBS -o "$CLAP_BIN"
echo "  -> $CLAP_BIN"
echo "Done."
