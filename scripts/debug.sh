#!/usr/bin/env bash
# scripts/debug.sh — Launch OpenOCD + GDB for VibeRTOS debugging
#
# Usage:
#   ./scripts/debug.sh --board rpi_pico --elf build/rpi_pico/hello_world/hello_world
#
# Opens two panes:
#   1. OpenOCD (GDB server on port 3333)
#   2. arm-none-eabi-gdb connected to OpenOCD
#
# Requires:
#   openocd with RP2040/RP2350 support
#   arm-none-eabi-gdb
#   (Optionally) tmux for split panes

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTOS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BOARD=""
ELF_PATH=""
GDB_PORT=3333

while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)    BOARD="$2"; shift 2 ;;
        --elf)      ELF_PATH="$2"; shift 2 ;;
        --port)     GDB_PORT="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 --board <name> --elf <path> [--port <n>]"
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ -z "$BOARD" || -z "$ELF_PATH" ]]; then
    echo "ERROR: --board and --elf required" >&2; exit 1
fi

if [[ ! -f "$ELF_PATH" ]]; then
    echo "ERROR: ELF not found: ${ELF_PATH}" >&2; exit 1
fi

# -----------------------------------------------------------------------
# Board-specific OpenOCD configuration
# -----------------------------------------------------------------------
case "$BOARD" in
    rpi_pico)
        OPENOCD_TARGET="target/rp2040.cfg"
        GDB_BINARY="arm-none-eabi-gdb"
        ;;
    rpi_pico2)
        OPENOCD_TARGET="target/rp2350.cfg"
        GDB_BINARY="arm-none-eabi-gdb"
        ;;
    *)
        echo "ERROR: Unknown board: ${BOARD}" >&2; exit 1 ;;
esac

OPENOCD_IFACE="interface/cmsis-dap.cfg"
OPENOCD_CMD="openocd -f ${OPENOCD_IFACE} -f ${OPENOCD_TARGET} \
    -c \"bindto 127.0.0.1\" \
    -c \"gdb_port ${GDB_PORT}\""

GDB_INIT_SCRIPT="${RTOS_ROOT}/scripts/.gdbinit_${BOARD}"

# Generate a temporary GDB init script
cat > "${GDB_INIT_SCRIPT}" << EOF
# Auto-generated GDB init for ${BOARD}
set pagination off
target remote localhost:${GDB_PORT}
monitor reset init
file ${ELF_PATH}
load
break vibe_init
continue
EOF

echo "=== VibeRTOS Debug Session ==="
echo "  Board:    ${BOARD}"
echo "  ELF:      ${ELF_PATH}"
echo "  GDB port: ${GDB_PORT}"
echo ""

# -----------------------------------------------------------------------
# Launch in tmux (preferred) or two terminals
# -----------------------------------------------------------------------
if command -v tmux &>/dev/null && [[ -n "${TMUX:-}" ]]; then
    echo "Launching OpenOCD + GDB in tmux panes..."

    # Create a horizontal split
    tmux split-window -h "${OPENOCD_CMD}"

    # Give OpenOCD a moment to start
    sleep 1

    # Run GDB in the original pane
    "${GDB_BINARY}" -x "${GDB_INIT_SCRIPT}" "${ELF_PATH}"

else
    echo "tmux not available or not in a tmux session."
    echo ""
    echo "  1. Start OpenOCD in a separate terminal:"
    echo "     ${OPENOCD_CMD}"
    echo ""
    echo "  2. In this terminal, run:"
    echo "     ${GDB_BINARY} -x ${GDB_INIT_SCRIPT} ${ELF_PATH}"
    echo ""
    echo "Starting OpenOCD in background (Ctrl+C to stop)..."
    eval "${OPENOCD_CMD} &"
    OPENOCD_PID=$!
    sleep 2

    "${GDB_BINARY}" -x "${GDB_INIT_SCRIPT}" "${ELF_PATH}" || true

    # Clean up
    kill "${OPENOCD_PID}" 2>/dev/null || true
fi

rm -f "${GDB_INIT_SCRIPT}"
