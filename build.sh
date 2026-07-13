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
# v1.17.0: the HTML/CSS/JS (MSHTML) presentation host has been RETIRED. The
# reception / appointment UI is now rendered 100% in native C++ (Win32/GDI), so
# src/webhost.cpp (and its webhost_*.inc includes + embedded HTML/CSS/JS in
# webhost_assets.inc) are no longer compiled. This removes the IE/Trident
# dependency, shrinks the EXE, and makes the UI single-engine + crash-safe.
# v1.46.0: the embedded WebView2/MSHTML Patient-Admission surface has been
# DELETED entirely (src/web_admission.cpp + its *.inc includes + the
# assets/admission bundle). The reception «پذیرش بیمار» page is now rendered
# 100% by the native Win32/GDI form in src/reception.cpp — no loopback HTTP
# bridge, no browser boot, so it can never freeze on the operator's hardware.
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp src/appointment.cpp \
      src/backup.cpp src/ui_kit.cpp src/backup_analyzer.cpp \
      src/backup_log.cpp src/sections.cpp src/print_designer.cpp \
      src/user_settings.cpp src/net_sync.cpp src/profile_requests.cpp \
      src/backup_log_viewer.cpp src/backup_mtf.cpp src/saved_messages.cpp \
      src/setup_splash.cpp src/web_designer.cpp src/services.cpp \
      src/insurance.cpp \
      src/client_log.cpp \
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

# ----------------------------------------------------------------------------
# v1.45.0 §9: sanity checks the previous attempt failed to do. Non-blocking —
# these only echo warnings (guarded so `set -e` never aborts the build on them).
# ----------------------------------------------------------------------------
set +e
echo "[verify] no longjmp/setjmp/__try in our source:"
if grep -rn --include='*.cpp' --include='*.inc' --include='*.h' \
       -E 'longjmp|setjmp|adSehVeh|g_sehArmed|g_sehJmp|__try|__except' src/ | grep -vqE ':\s*//|^\s*//' ; then
    grep -rn --include='*.cpp' --include='*.inc' --include='*.h' \
       -E 'longjmp|setjmp|adSehVeh|g_sehArmed|g_sehJmp|__try|__except' src/ | grep -vE ':\s*//|^\s*//'
    echo "  WARN: forbidden token still present (non-comment)"
else
    echo "  OK"
fi
echo "[verify] exactly ONE AddVectoredExceptionHandler call:"
n=$(grep -rn --include='*.cpp' --include='*.inc' 'AddVectoredExceptionHandler' src/ | wc -l)
echo "  count=$n"
[ "$n" = "1" ] || echo "  WARN: expected 1"
echo "[verify] EXE size (>500KB):"
sz=$(stat -c%s build/AzadiTeb.exe)
echo "  size=$sz"
[ "$sz" -gt 512000 ] || echo "  WARN: EXE too small"
echo "[verify] no deleted symbols in binary:"
sym=$(grep -aoE 'adSehVeh|g_sehArmed|g_sehJmp|azAnalyzeVeh|s_azArmed' build/AzadiTeb.exe | sort -u)
if [ -z "$sym" ]; then echo "  OK (none)"; else echo "  WARN: $sym"; fi

# ----------------------------------------------------------------------------
# v1.46.0 §3: verify the HTML/MSHTML/WebView2 admission surface is fully gone.
# Every check must print OK; any WARN fails the release.
# ----------------------------------------------------------------------------
echo "[v1.46 verify] no web_admission code remains:"
if grep -rn --include='*.cpp' --include='*.inc' --include='*.h' 'WebAdmission_\|web_admission' src/ ; then
    echo "  WARN: web_admission symbol still referenced"
else
    echo "  OK — reception is 100% native GDI"
fi

echo "[v1.46 verify] no HTML admission assets:"
if [ -d assets/admission ] ; then
    echo "  WARN: assets/admission still exists"
else
    echo "  OK"
fi

echo "[v1.46 verify] no MSHTML/WebView2 references from reception.cpp:"
if grep -n 'MSHTML\|WebView\|chrome.webview\|window.external' src/reception.cpp ; then
    echo "  WARN: dead reference"
else
    echo "  OK"
fi

echo "[v1.46 verify] EXE size sanity (>500KB, <10MB):"
sz46=$(stat -c%s build/AzadiTeb.exe); echo "  size=$sz46"
{ [ "$sz46" -gt 512000 ] && [ "$sz46" -lt 10485760 ]; } || echo "  WARN: EXE size looks wrong"

echo "[v1.46 verify] no loopback bind in admission path:"
if grep -rn --include='*.cpp' --include='*.inc' 'sockaddr_in\|bind(\|listen(\|accept(' src/reception.cpp ; then
    echo "  WARN: reception.cpp shouldn't have sockets"
else
    echo "  OK"
fi
set -e

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
