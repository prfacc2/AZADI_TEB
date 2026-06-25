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
# Release 1.4.0 sources (new: sections, print_designer*, user_settings,
# net_sync, profile_requests, backup_log_viewer).
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp src/ui_kit.cpp src/backup_analyzer.cpp \
      src/backup_log.cpp src/sections.cpp src/print_designer.cpp \
      src/user_settings.cpp src/net_sync.cpp src/profile_requests.cpp \
      src/backup_log_viewer.cpp src/backup_mtf.cpp src/saved_messages.cpp \
      src/webhost.cpp src/setup_splash.cpp"

$CXX -std=c++17 -O2 -s -municode -mwindows \
    -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 \
    -static -static-libgcc -static-libstdc++ \
    -Wall -Wextra -Werror \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-misleading-indentation -Wno-unused-function \
    -Wno-missing-field-initializers \
    $SRCS obj/app.res \
    -o build/AzadiTeb.exe \
    -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
    -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
    -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
    -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi

echo "[3/3] Stripping..."
i686-w64-mingw32-strip build/AzadiTeb.exe

# Drop a SHA-256 sidecar next to the exe (used by the in-app updater / verify).
if command -v sha256sum >/dev/null 2>&1; then
    ( cd build && printf '%s  AzadiTeb.exe\n' "$(sha256sum AzadiTeb.exe | awk '{print $1}')" > AzadiTeb.exe.sha256 )
    echo "SHA-256 -> build/AzadiTeb.exe.sha256"
fi

ls -lh build/AzadiTeb.exe
echo "Build OK -> build/AzadiTeb.exe"

# ----------------------------------------------------------------------------
# §D.5: OPTIONAL headless print-designer smoke test. Gated behind AZ_SMOKE so
# production builds skip it entirely. Builds a SEPARATE debug binary with the
# AZ_DEBUG_BUILD hook, runs it under Wine with AZ_DEBUG_SCREEN=print_designer
# (which seeds the section/design stores and exits 0 if the open path is
# healthy), and fails the build on a non-zero exit code. The production
# build/AzadiTeb.exe above is NOT affected by this debug binary.
# ----------------------------------------------------------------------------
if [ -n "$AZ_SMOKE" ]; then
    echo "[smoke] Building debug binary for print_designer smoke test..."
    mkdir -p obj
    $CXX -std=c++17 -O2 -municode -mwindows \
        -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 -DAZ_DEBUG_BUILD \
        -static -static-libgcc -static-libstdc++ \
        -Wall -Wextra \
        -Wno-unused-variable -Wno-unused-parameter \
        -Wno-misleading-indentation -Wno-unused-function \
        -Wno-missing-field-initializers \
        $SRCS obj/app.res \
        -o build/AzadiTeb_smoke.exe \
        -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
        -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
        -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
        -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi
    if command -v wine >/dev/null 2>&1; then
        echo "[smoke] Running print_designer open/close path under Wine..."
        AZ_DEBUG_SCREEN=print_designer wine build/AzadiTeb_smoke.exe
        rc=$?
        if [ "$rc" -ne 0 ]; then
            echo "[smoke] FAILED: print_designer smoke exited $rc" >&2
            exit "$rc"
        fi
        echo "[smoke] print_designer smoke PASSED"
    else
        echo "[smoke] Wine not available — debug binary built but not executed."
    fi
    rm -f build/AzadiTeb_smoke.exe
fi
