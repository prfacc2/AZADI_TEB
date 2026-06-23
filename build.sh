#!/bin/bash
# ============================================================================
#  Azadi-Teb build script (cross-compile from Linux with MinGW-w64)
#  Output: build/AzadiTeb.exe  — single 32-bit exe that runs on BOTH
#  x86 and x64 Windows (7 / 8 / 8.1 / 10 / 11+), fully static (no DLLs).
# ============================================================================
set -e
cd "$(dirname "$0")"

CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres

mkdir -p build obj

echo "[1/3] Compiling resources..."
$RES -O coff -i src/app.rc -o obj/app.res

echo "[2/3] Compiling C++..."
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp src/ui_kit.cpp src/backup_analyzer.cpp \
      src/backup_log.cpp"

$CXX -std=c++17 -O2 -municode -mwindows \
    -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 \
    -static -static-libgcc -static-libstdc++ \
    -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter \
    -Wno-misleading-indentation -Wno-unused-function \
    -Wno-missing-field-initializers \
    $SRCS obj/app.res \
    -o build/AzadiTeb.exe \
    -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
    -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
    -lole32 -luuid -lversion -lwinmm -ldbghelp

echo "[3/3] Stripping..."
i686-w64-mingw32-strip build/AzadiTeb.exe

# Drop a SHA-256 sidecar next to the exe (used by the in-app updater / verify).
if command -v sha256sum >/dev/null 2>&1; then
    ( cd build && printf '%s  AzadiTeb.exe\n' "$(sha256sum AzadiTeb.exe | awk '{print $1}')" > AzadiTeb.exe.sha256 )
    echo "SHA-256 -> build/AzadiTeb.exe.sha256"
fi

ls -lh build/AzadiTeb.exe
echo "Build OK -> build/AzadiTeb.exe"
