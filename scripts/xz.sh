#!/usr/bin/env bash
#
# Build / flash / monitor xiaozhi for a specific ESP32-S3 board variant.
#
#   XZ_BOARD=supermini ./scripts/xz.sh flash monitor
#   XZ_BOARD=n16r8     ./scripts/xz.sh build
#   XZ_BOARD=n16r8 XZ_PORT=/dev/cu.usbmodem5C930658041 ./scripts/xz.sh flash
#   XZ_BOARD=supermini ./scripts/xz.sh menuconfig
#
# Board profiles live in sdkconfig.board.<name> and are layered on top of
# sdkconfig.defaults + sdkconfig.defaults.esp32s3. Each board keeps its own
# sdkconfig and build directory, so switching boards does not clobber the
# other one's configuration or force a full rebuild.
#
# Env:
#   XZ_BOARD  required, e.g. supermini | n16r8
#   XZ_PORT   optional, serial port; auto-detected when exactly one is present
#   IDF_ACTIVATE_SCRIPT  optional, path to activate_idf_*.sh

set -eu

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

list_boards() {
    for f in sdkconfig.board.*; do
        [ -f "$f" ] || continue
        echo "  ${f#sdkconfig.board.}"
    done
}

BOARD="${XZ_BOARD:-}"
if [ -z "$BOARD" ]; then
    echo "XZ_BOARD is not set. Available boards:" >&2
    list_boards >&2
    exit 1
fi

PROFILE="sdkconfig.board.${BOARD}"
if [ ! -f "$PROFILE" ]; then
    echo "No such board profile: $PROFILE" >&2
    echo "Available boards:" >&2
    list_boards >&2
    exit 1
fi

# Locate the ESP-IDF environment. If idf.py is already on PATH we use it as is;
# otherwise we run it through the ESP-IDF Installation Manager's activation
# script. That script is generated for zsh/bash by EIM but is not compatible
# with bash 3.2 (still the system bash on macOS), so it is sourced inside zsh
# when available.
ACTIVATE=""
if ! command -v idf.py >/dev/null 2>&1; then
    ACTIVATE="${IDF_ACTIVATE_SCRIPT:-}"
    if [ -z "$ACTIVATE" ]; then
        ACTIVATE=$(ls -1 "$HOME"/.espressif/tools/activate_idf_*.sh 2>/dev/null | sort | tail -1 || true)
    fi
    if [ -z "$ACTIVATE" ] || [ ! -f "$ACTIVATE" ]; then
        echo "idf.py not found, and no activate_idf_*.sh under ~/.espressif/tools." >&2
        echo "Activate ESP-IDF first, or set IDF_ACTIVATE_SCRIPT." >&2
        exit 1
    fi
fi

# Pick the serial port: explicit override, otherwise the only one attached.
PORT="${XZ_PORT:-}"
if [ -z "$PORT" ]; then
    found=""
    count=0
    for p in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
        [ -e "$p" ] || continue
        found="$p"
        count=$((count + 1))
    done
    if [ "$count" -eq 1 ]; then
        PORT="$found"
    elif [ "$count" -gt 1 ]; then
        echo "Multiple serial ports attached; set XZ_PORT to one of:" >&2
        for p in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
            [ -e "$p" ] && echo "  $p" >&2
        done
        exit 1
    fi
fi

BUILD_DIR="build/${BOARD}"
SDKCONFIG="sdkconfig.${BOARD}"

echo "board=${BOARD}  port=${PORT:-<none>}  sdkconfig=${SDKCONFIG}  build=${BUILD_DIR}"

set -- -B "$BUILD_DIR" \
    -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=${SDKCONFIG}" \
    -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32s3;${PROFILE}" \
    ${PORT:+-p "$PORT"} \
    "$@"

if [ -z "$ACTIVATE" ]; then
    exec idf.py "$@"
fi

if command -v zsh >/dev/null 2>&1; then
    exec zsh -c '. "$1" >/dev/null; shift; exec idf.py "$@"' zsh "$ACTIVATE" "$@"
fi

# No zsh: try bash anyway, tolerating the script's unset shell-detect vars.
set +u
# shellcheck disable=SC1090
. "$ACTIVATE" >/dev/null
set -u
exec idf.py "$@"
