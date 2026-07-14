#!/bin/bash
# ============================================================================
#  Azadi-Teb v2 build script
#  Builds the new Avalonia UI (.NET 9) desktop client as a single self-contained
#  Windows x64 EXE and drops it into build/AzadiTeb.exe.
#
#  Requires: .NET 9 SDK (see .dotnet-env.sh for the sandbox toolchain paths).
#  Usage:    ./build.sh            (release single-file win-x64)
#            ./build.sh core       (also build the C++ Core REST engine)
# ============================================================================
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"

# Load dotnet toolchain env if present (sandbox); otherwise assume PATH dotnet.
[ -f "$ROOT/.dotnet-env.sh" ] && source "$ROOT/.dotnet-env.sh"

UI_DIR="$ROOT/app/AzadiTeb.UI"
OUT_DIR="$ROOT/build"
mkdir -p "$OUT_DIR"

echo "[1/3] Cleaning previous build output..."
rm -f "$OUT_DIR/AzadiTeb.exe" "$OUT_DIR/AzadiTeb.exe.sha256"

echo "[2/3] Publishing Avalonia UI (win-x64, self-contained, single file)..."
dotnet publish "$UI_DIR" -c Release -r win-x64 --self-contained true \
    -p:PublishSingleFile=true \
    -p:IncludeNativeLibrariesForSelfExtract=true \
    -p:EnableCompressionInSingleFile=true \
    -p:DebugType=none -p:DebugSymbols=false \
    -o "$OUT_DIR/_publish"

cp "$OUT_DIR/_publish/AzadiTeb.exe" "$OUT_DIR/AzadiTeb.exe"
rm -rf "$OUT_DIR/_publish"

echo "[3/3] Writing checksum..."
( cd "$OUT_DIR" && sha256sum AzadiTeb.exe | awk '{print $1}' > AzadiTeb.exe.sha256 )

# Optional C++ Core REST engine
if [ "$1" = "core" ]; then
    echo "[+] Building C++ Core REST engine..."
    cmake -S "$ROOT/backend" -B "$ROOT/backend/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/backend/build" -j"$(nproc)"
    ctest --test-dir "$ROOT/backend/build" --output-on-failure || true
fi

echo "Done. Output: $OUT_DIR/AzadiTeb.exe"
ls -la "$OUT_DIR/AzadiTeb.exe"
