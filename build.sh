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

# ----------------------------------------------------------------------------
#  v1.48.0: publish the Avalonia (.NET) Patient-Reception surface FIRST so it
#  can be embedded as RCDATA(700) into the native exe. The reception «پذیرش
#  بیمار» APPEARANCE is now this Avalonia UI, embedded (reparented) inside the
#  reception tab and driven over the SAME loopback /api bridge the retired HTML
#  page used — the C++ core is unchanged.
#
#  If the .NET SDK is not available (or AZ_SKIP_AVALONIA=1), we write a 1-byte
#  placeholder so app.rc still compiles; the app then cleanly falls back to the
#  HTML/native reception engine at runtime. This keeps the pure-MinGW build path
#  working on machines without .NET.
# ----------------------------------------------------------------------------
AVALONIA_PROJ="avalonia/AzadiTeb.Reception/AzadiTeb.Reception.csproj"
AVALONIA_OUT="build/AzadiTeb.Reception.exe"
if [ -z "$AZ_SKIP_AVALONIA" ] && command -v dotnet >/dev/null 2>&1 && [ -f "$AVALONIA_PROJ" ]; then
    echo "[0/3] Publishing Avalonia reception surface (.NET, win-x86 self-contained)..."
    dotnet publish "$AVALONIA_PROJ" -c Release -r win-x86 --self-contained \
        -o avalonia/AzadiTeb.Reception/publish >/dev/null
    cp -f avalonia/AzadiTeb.Reception/publish/AzadiTeb.Reception.exe "$AVALONIA_OUT"
    echo "     -> $AVALONIA_OUT ($(du -h "$AVALONIA_OUT" | awk '{print $1}'))"
else
    if [ ! -s "$AVALONIA_OUT" ]; then
        printf '\0' > "$AVALONIA_OUT"    # 1-byte placeholder → runtime fallback
        echo "[0/3] Avalonia build skipped — embedding placeholder (HTML fallback)."
    fi
fi

echo "[1/3] Compiling resources..."
$RES -O coff -i src/app.rc -o obj/app.res

echo "[2/3] Compiling C++..."
# Release 1.4.0 sources (new: sections, print_designer*, user_settings,
# net_sync, profile_requests, backup_log_viewer).
# v1.17.0: the HTML/CSS/JS (MSHTML) presentation host has been RETIRED. The
# reception / appointment UI is now rendered 100% in native C++ (Win32/GDI), so
# src/webhost.cpp (and its webhost_*.inc includes + embedded HTML/CSS/JS in
# webhost_assets.inc) are no longer compiled. This removes the IE/Trident
# dependency, shrinks the EXE, and makes the UI single-engine + crash-safe.
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp src/ui_kit.cpp src/backup_analyzer.cpp \
      src/backup_log.cpp src/sections.cpp src/print_designer.cpp \
      src/user_settings.cpp src/net_sync.cpp src/profile_requests.cpp \
      src/backup_log_viewer.cpp src/backup_mtf.cpp src/saved_messages.cpp \
      src/setup_splash.cpp src/web_designer.cpp src/services.cpp \
      src/web_admission.cpp src/av_reception.cpp \
      src/web_pages.cpp src/web_thread_pool.cpp src/web_ping_api.cpp"

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
    -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32

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
        -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32
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
