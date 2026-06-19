#!/bin/bash
# Build a DEBUG exe and capture screenshots of multiple screens under Wine+Xvfb.
# Usage: ./shots.sh [screen1 screen2 ...]   (default: home reception manage admin settings backup shift)
set -e
cd "$(dirname "$0")"
CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres
export WINEDEBUG=-all
export WINEPREFIX="$HOME/.wine"
export DISPLAY=:99
RESO="${RESO:-1600x900}"
SCREENS=("$@")
if [ ${#SCREENS[@]} -eq 0 ]; then
  SCREENS=(home reception manage admin settings backup shift)
fi

mkdir -p obj shots
echo "[build dbg] resources..."
$RES -O coff -i src/app.rc -o obj/app.res
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp"
echo "[build dbg] compiling..."
$CXX -std=c++17 -O2 -municode -mwindows -DAZ_DEBUG_BUILD \
    -D_WIN32_IE=0x0700 -static -static-libgcc -static-libstdc++ \
    -Wall -Wno-unused-variable -Wno-misleading-indentation -Wno-unused-function \
    $SRCS obj/app.res \
    -o build/AzadiTeb_dbg.exe \
    -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -luser32 -lshlwapi -lwininet \
    -ladvapi32 -lshell32 -lwinspool -lole32 -luuid >/tmp/dbgbuild.log 2>&1 || { tail -30 /tmp/dbgbuild.log; exit 1; }
i686-w64-mingw32-strip build/AzadiTeb_dbg.exe
echo "[build dbg] OK"

# ensure Xvfb
if ! pgrep -f "Xvfb :99" >/dev/null; then
  Xvfb :99 -screen 0 ${RESO}x24 >/tmp/xvfb.log 2>&1 &
  sleep 3
fi

for SCREEN in "${SCREENS[@]}"; do
  OUT="shots/shot_${SCREEN}.png"
  export AZ_DEBUG_SCREEN="$SCREEN"
  wine build/AzadiTeb_dbg.exe >/dev/null 2>&1 &
  WPID=$!
  sleep 8
  import -window root "$OUT" 2>/dev/null || xwd -root -silent -display :99 | convert xwd:- "$OUT" 2>/dev/null || true
  # kill the wine app cleanly
  kill $WPID 2>/dev/null || true
  wineserver -k 2>/dev/null || true
  sleep 1
  if [ -f "$OUT" ]; then echo "Saved $OUT ($(stat -c%s "$OUT") bytes)"; else echo "NO SHOT for $SCREEN"; fi
done
echo "DONE"
