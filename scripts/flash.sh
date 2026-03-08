#!/usr/bin/env bash
# scripts/flash.sh — Flash a VibeRTOS binary to a target board
#
# Usage:
#   ./scripts/flash.sh --board rpi_pico --bin build/rpi_pico/hello_world/hello_world.bin
#   ./scripts/flash.sh --board rpi_pico2 --elf build/rpi_pico2/blinky/blinky
#
# Supported methods:
#   rpi_pico / rpi_pico2: picotool (USB mass storage) or openocd (SWD)
#
# Requires:
#   picotool: https://github.com/raspberrypi/picotool
#   openocd:  https://openocd.org (with RP2040/RP2350 target support)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTOS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BOARD=""
BIN_PATH=""
ELF_PATH=""
METHOD="auto"   # auto | picotool | openocd

while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)    BOARD="$2"; shift 2 ;;
        --bin)      BIN_PATH="$2"; shift 2 ;;
        --elf)      ELF_PATH="$2"; shift 2 ;;
        --method)   METHOD="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 --board <name> [--bin <path>|--elf <path>] [--method auto|picotool|openocd]"
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ -z "$BOARD" ]]; then
    echo "ERROR: --board required" >&2; exit 1
fi

# -----------------------------------------------------------------------
# Flash using picotool (USB mass storage / UF2)
# -----------------------------------------------------------------------
flash_picotool() {
    local file="$1"
    echo "=== Flashing with picotool ==="

    if ! command -v picotool &>/dev/null; then
        echo "ERROR: picotool not found. Install from https://github.com/raspberrypi/picotool" >&2
        exit 1
    fi

    echo "  File: ${file}"
    echo ""
    echo "  Hold BOOTSEL button on the Pico while connecting USB."
    echo "  Press Enter when ready..."
    read -r

    if [[ "$file" == *.bin ]]; then
        picotool load "${file}" --force
    elif [[ "$file" == *.elf ]]; then
        picotool load "${file}" --force
    else
        echo "ERROR: unsupported file type: ${file}" >&2; exit 1
    fi

    picotool reboot
    echo "=== Flash complete ==="
}

# -----------------------------------------------------------------------
# Flash using OpenOCD + SWD
# -----------------------------------------------------------------------
flash_openocd() {
    local file="$1"
    echo "=== Flashing with OpenOCD (SWD) ==="

    if ! command -v openocd &>/dev/null; then
        echo "ERROR: openocd not found" >&2; exit 1
    fi

    # Board-specific OpenOCD target
    local target_cfg
    case "$BOARD" in
        rpi_pico)   target_cfg="target/rp2040.cfg" ;;
        rpi_pico2)  target_cfg="target/rp2350.cfg" ;;
        *)          echo "ERROR: Unknown board ${BOARD}" >&2; exit 1 ;;
    esac

    local interface_cfg="interface/cmsis-dap.cfg"

    if [[ "$file" == *.elf ]]; then
        openocd -f "${interface_cfg}" -f "${target_cfg}" \
            -c "program ${file} verify reset exit"
    elif [[ "$file" == *.bin ]]; then
        openocd -f "${interface_cfg}" -f "${target_cfg}" \
            -c "program ${file} 0x10000000 verify reset exit"
    fi

    echo "=== Flash complete ==="
}

# -----------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------
FLASH_FILE="${BIN_PATH:-${ELF_PATH}}"
if [[ -z "$FLASH_FILE" ]]; then
    echo "ERROR: --bin or --elf required" >&2; exit 1
fi

if [[ ! -f "$FLASH_FILE" ]]; then
    echo "ERROR: file not found: ${FLASH_FILE}" >&2; exit 1
fi

case "$METHOD" in
    auto)
        if command -v picotool &>/dev/null; then
            flash_picotool "$FLASH_FILE"
        else
            flash_openocd "$FLASH_FILE"
        fi ;;
    picotool) flash_picotool "$FLASH_FILE" ;;
    openocd)  flash_openocd "$FLASH_FILE" ;;
    *)        echo "ERROR: unknown method: ${METHOD}" >&2; exit 1 ;;
esac
