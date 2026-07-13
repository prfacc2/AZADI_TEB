#!/bin/bash
# Build a DEBUG exe (AZ_DEBUG_BUILD) that can jump straight to a screen,
# run it under Wine + Xvfb, and grab a PNG screenshot.
set -e
cd "$(dirname "$0")"
SCREEN="${1:-home}"     # home / reception / manage / settings
OUT="${2:-/home/user/webapp/shot_${SCREEN}.png}"
RESO="${3:-1600x900}"   # screen resolution for Xvfb
CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres
mkdir -p obj
$RES -O coff -i src/app.rc -o obj/app.res
# v1.4.0: keep the source/lib list in sync with build.sh (new modules added).
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp src/ui_kit.cpp src/backup_analyzer.cpp src/backup_log.cpp \
      src/sections.cpp src/print_designer.cpp src/user_settings.cpp \
      src/net_sync.cpp src/profile_requests.cpp src/backup_log_viewer.cpp \
      src/backup_mtf.cpp src/saved_messages.cpp src/setup_splash.cpp src/web_designer.cpp src/services.cpp \
      src/insurance.cpp src/client_log.cpp \
      src/web_pages.cpp src/web_thread_pool.cpp src/web_ping_api.cpp"
$CXX -std=c++17 -O2 -municode -mwindows -DAZ_DEBUG_BUILD \
    -D_WIN32_IE=0x0700 -static -static-libgcc -static-libstdc++ \
    -Wall -Wno-unused-variable $SRCS obj/app.res \
    -o build/AzadiTeb_dbg.exe \
    -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
    -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
    -lole32 -luuid -lversion -lwinmm -ldbghelp \
    -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32 -loleaut32 >/dev/null
i686-w64-mingw32-strip build/AzadiTeb_dbg.exe

export WINEDEBUG=-all
export AZ_DEBUG_SCREEN="$SCREEN"
export DISPLAY=:99
pkill -f Xvfb 2>/dev/null || true
sleep 0.5
Xvfb :99 -screen 0 ${RESO}x24 >/dev/null 2>&1 &
XVFB=$!
sleep 1.5
wine build/AzadiTeb_dbg.exe >/dev/null 2>&1 &
WPID=$!
sleep 9
import -window root "$OUT" 2>/dev/null || xwd -root -silent | convert xwd:- "$OUT" 2>/dev/null || \
  python3 -c "from PIL import ImageGrab; ImageGrab.grab().save('$OUT')" 2>/dev/null || true
# fallback: use scrot or imagemagick
if [ ! -f "$OUT" ]; then
  DISPLAY=:99 import -window root "$OUT" 2>/dev/null || true
fi
sleep 0.5
pkill -f AzadiTeb_dbg 2>/dev/null || true
kill $XVFB 2>/dev/null || true
echo "Saved $OUT"
ls -la "$OUT" 2>/dev/null || echo "NO SCREENSHOT"
