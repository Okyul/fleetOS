#!/usr/bin/env bash
# Bump firmware/main/main.c to a specific version + blink rate, build, and
# stage the resulting .bin into firmware-builds/ ready to upload to R2.
#
# Usage:
#   ./tools/release.sh <version> [<half-period-ms>]
#
# Example — build a v4.0.0 image with a 50ms half-period (10Hz blink):
#   ./tools/release.sh 4.0.0 50
#
# Defaults:
#   version          — required, no default
#   half-period-ms   — keeps whatever is currently in main.c (no edit)
#
# Run from inside WSL Ubuntu, from the project root. Calls tools/build.sh
# under the hood.

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <version> [<half-period-ms>]" >&2
    exit 2
fi

VERSION="$1"
HALF_PERIOD="${2:-}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_C="$REPO_ROOT/firmware/main/main.c"
OUT_DIR="$REPO_ROOT/firmware-builds"
OUT="$OUT_DIR/fleetos-v${VERSION}.bin"

if [[ ! -f "$MAIN_C" ]]; then
    echo "error: $MAIN_C not found" >&2
    exit 1
fi

# Patch FIRMWARE_VERSION. Single source of truth, single sed.
sed -i -E "s|^#define FIRMWARE_VERSION    \".*\"|#define FIRMWARE_VERSION    \"${VERSION}\"|" "$MAIN_C"

if [[ -n "$HALF_PERIOD" ]]; then
    sed -i -E "s|^#define BLINK_HALF_PERIOD_MS [0-9]+|#define BLINK_HALF_PERIOD_MS ${HALF_PERIOD}|" "$MAIN_C"
fi

echo "==> patched main.c:"
grep -E "^#define (FIRMWARE_VERSION|BLINK_HALF_PERIOD_MS)" "$MAIN_C"

echo "==> building..."
"$REPO_ROOT/tools/build.sh" build > /dev/null

# build.sh syncs firmware/ into ~/fleetos-poc/firmware/, so the binary is there.
SRC_BIN="$HOME/fleetos-poc/firmware/build/fleetos.bin"
if [[ ! -f "$SRC_BIN" ]]; then
    echo "error: build did not produce $SRC_BIN" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
cp "$SRC_BIN" "$OUT"
ls -la "$OUT"

cat <<EOF

==> next steps:
  1. Upload $OUT to R2 (drag-drop in the Cloudflare console).
  2. Copy the public URL.
  3. Push to a device:
       python host/fleetctl.py cmd <device-id> '{"url":"<public-url>"}'
  4. Watch the device on:
       python host/fleetctl.py status

EOF
