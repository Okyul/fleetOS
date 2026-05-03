#!/usr/bin/env bash
# Sync firmware/ from this Dropbox-backed path into ~/fleetos-poc/ (a path
# without spaces or parens — ESP-IDF/CMake choke on them) and then build.
#
# Run from inside WSL Ubuntu. From the project root:
#   ./tools/build.sh           # builds
#   ./tools/build.sh flash     # builds + flashes (needs board attached via usbipd)
#   ./tools/build.sh monitor   # build + flash + serial monitor
#
# Source of truth lives in Dropbox; we mirror just before each build so edits
# are picked up. Build artifacts stay in ~/fleetos-poc/firmware/build (gitignored).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$HOME/fleetos-poc"
EXPORT_SH="$HOME/esp/esp-idf/export.sh"

if [[ ! -f "$EXPORT_SH" ]]; then
    echo "error: $EXPORT_SH not found — run install.sh first" >&2
    exit 1
fi

mkdir -p "$WORK_DIR"
rsync -a --delete \
    --exclude build --exclude managed_components \
    --exclude sdkconfig --exclude sdkconfig.old \
    "$REPO_ROOT/firmware" "$WORK_DIR/"

# shellcheck disable=SC1090
source "$EXPORT_SH" > /dev/null

cd "$WORK_DIR/firmware"
idf.py "${@:-build}"
