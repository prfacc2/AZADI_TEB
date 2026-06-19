#!/bin/bash
# ============================================================================
#  Azadi-Teb (آزادی‌طب) — source backup script
# ----------------------------------------------------------------------------
#  Creates a versioned, dated ZIP archive of the project SOURCE (and docs /
#  assets / fonts / build scripts) under  backup/  so every release can be
#  restored later. The archive name follows the existing convention:
#
#      AzadiTeb_v<VERSION>_source_backup_<YYYY-MM-DD>.zip
#
#  The VERSION is read automatically from src/app.h (APP_VERSION_W) unless you
#  pass one explicitly:
#
#      ./scripts/backup.sh              # auto-detect version + today's date
#      ./scripts/backup.sh 1.8.0        # force a version label
#      ./scripts/backup.sh 1.8.0 2026-06-18   # force version + date
#      ./scripts/backup.sh --with-assets      # also include the heavy
#                                             # assets/ background images
#
#  LIVE PATIENT-DATA backup (instant, downloadable):
#
#      ./scripts/backup.sh --data       # package the live data/ dir RIGHT NOW
#      ./scripts/backup.sh --live       # (alias of --data)
#      ./scripts/backup.sh --data /path/to/data   # custom data dir
#
#  In --data/--live mode the script ignores the source tree entirely and just
#  snapshots the runtime  data/  directory (pipe-delimited .dat files, patient
#  records, attachments, presence) into a timestamped archive so the user can
#  download an up-to-the-moment copy of all clinic data. The version label is
#  still auto-detected from src/app.h for the file name.
#
#  By DEFAULT (matching the historical lightweight backups) the archive holds
#  the CODE side of the project only:
#      src/  docs/  fonts/  update/  scripts/  build.sh  shot.sh  README*.md
#  The big assets/ background images (~8 MB of PNGs) are EXCLUDED unless you
#  pass --with-assets (then a *_full_* archive is produced instead).
#
#  Always EXCLUDED:    build/  obj/  .git/  backup/  data/  logs/
#                      (compiled output, VCS internals and previous backups)
# ============================================================================
set -e

# ---- optional flags (can appear anywhere) -----------------------------------
WITH_ASSETS=0
DATA_MODE=0
ARGS=()
for a in "$@"; do
    case "$a" in
        --with-assets) WITH_ASSETS=1 ;;
        --data|--live) DATA_MODE=1 ;;
        *) ARGS+=("$a") ;;
    esac
done
set -- "${ARGS[@]}"

# always operate from the project root (parent of this script's dir)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

# ---- resolve version --------------------------------------------------------
# In data mode the first positional arg may be a custom data dir (it starts with
# '/' or '.' or names an existing dir), so it must NOT be consumed as a version.
# We therefore ALWAYS auto-detect the version from src/app.h in --data mode.
if [ "$DATA_MODE" -eq 1 ]; then
    VERSION=""
else
    VERSION="$1"
fi
if [ -z "$VERSION" ]; then
    # pull L"x.y.z" out of:  #define APP_VERSION_W   L"1.9.0"
    VERSION="$(grep -oE 'APP_VERSION_W[[:space:]]+L"[0-9]+\.[0-9]+\.[0-9]+"' src/app.h \
               | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
fi
if [ -z "$VERSION" ]; then
    echo "!! could not detect version from src/app.h — pass it explicitly:"
    echo "   ./scripts/backup.sh 1.8.0"
    exit 1
fi

# ---- resolve date -----------------------------------------------------------
if [ "$DATA_MODE" -eq 1 ]; then
    # include a precise timestamp so re-runs are distinct "live" snapshots
    DATE="$(date +%Y-%m-%d_%H%M%S)"
else
    DATE="$2"
    [ -z "$DATE" ] && DATE="$(date +%Y-%m-%d)"
fi

OUT_DIR="backup"
mkdir -p "$OUT_DIR"

# =============================================================================
#  LIVE PATIENT-DATA MODE — snapshot the runtime data/ dir RIGHT NOW
# =============================================================================
if [ "$DATA_MODE" -eq 1 ]; then
    # data dir: first positional arg, else default ./data
    DATA_DIR="$1"
    [ -z "$DATA_DIR" ] && DATA_DIR="data"
    if [ ! -d "$DATA_DIR" ]; then
        echo "!! data directory not found: $DATA_DIR"
        echo "   pass it explicitly:  ./scripts/backup.sh --data /path/to/data"
        exit 1
    fi
    ARCHIVE="$OUT_DIR/AzadiTeb_v${VERSION}_data_backup_${DATE}.zip"
    echo "============================================================"
    echo "  Azadi-Teb LIVE data backup"
    echo "  version : v${VERSION}"
    echo "  data    : ${DATA_DIR}"
    echo "  date    : ${DATE}"
    echo "  output  : ${ARCHIVE}"
    echo "============================================================"
    [ -f "$ARCHIVE" ] && { echo "  (replacing existing archive)"; rm -f "$ARCHIVE"; }
    if command -v zip >/dev/null 2>&1; then
        # store the data dir at the archive root as data/
        ( cd "$(dirname "$DATA_DIR")" && \
          zip -r -q "$ROOT_DIR/$ARCHIVE" "$(basename "$DATA_DIR")" )
    else
        echo "!! 'zip' not found; falling back to tar.gz"
        ARCHIVE="${ARCHIVE%.zip}.tar.gz"
        tar -czf "$ARCHIVE" -C "$(dirname "$DATA_DIR")" "$(basename "$DATA_DIR")"
    fi
    SIZE="$(du -h "$ARCHIVE" | cut -f1)"
    echo "------------------------------------------------------------"
    echo "  ✅ LIVE data backup created: $ARCHIVE  ($SIZE)"
    echo "  (ready to download)"
    echo "------------------------------------------------------------"
    exit 0
fi

if [ "$WITH_ASSETS" -eq 1 ]; then
    ARCHIVE="$OUT_DIR/AzadiTeb_v${VERSION}_full_backup_${DATE}.zip"
else
    ARCHIVE="$OUT_DIR/AzadiTeb_v${VERSION}_source_backup_${DATE}.zip"
fi

echo "============================================================"
echo "  Azadi-Teb backup"
echo "  version : v${VERSION}"
echo "  date    : ${DATE}"
echo "  assets  : $([ "$WITH_ASSETS" -eq 1 ] && echo included || echo excluded)"
echo "  output  : ${ARCHIVE}"
echo "============================================================"

# remove a same-named archive so re-runs are idempotent
[ -f "$ARCHIVE" ] && { echo "  (replacing existing archive)"; rm -f "$ARCHIVE"; }

# ---- build the include list (only paths that actually exist) -----------------
INCLUDE=()
PATHS=(src docs fonts update scripts build.sh shot.sh \
       README.md README_INIT.md .gitignore)
[ "$WITH_ASSETS" -eq 1 ] && PATHS+=(assets)
for p in "${PATHS[@]}"; do
    [ -e "$p" ] && INCLUDE+=("$p")
done

# zip with sensible exclusions (defensive — most aren't in the include list)
if command -v zip >/dev/null 2>&1; then
    zip -r -q "$ARCHIVE" "${INCLUDE[@]}" \
        -x '*/build/*' '*/obj/*' '*/.git/*' '*/backup/*' \
           '*/data/*' '*/logs/*' '*.o' '*.res' '*.exe'
else
    echo "!! 'zip' not found; falling back to tar.gz"
    ARCHIVE="${ARCHIVE%.zip}.tar.gz"
    tar --exclude='*/build/*' --exclude='*/obj/*' --exclude='*/.git/*' \
        --exclude='*/backup/*' --exclude='*/data/*' --exclude='*/logs/*' \
        -czf "$ARCHIVE" "${INCLUDE[@]}"
fi

SIZE="$(du -h "$ARCHIVE" | cut -f1)"
echo "------------------------------------------------------------"
echo "  ✅ backup created: $ARCHIVE  ($SIZE)"
echo "------------------------------------------------------------"
