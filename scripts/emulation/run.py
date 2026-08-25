#!/usr/bin/env python3
"""run.py — run VibeRTOS in QEMU (ARM MPS2 AN386).

Usage:
  ./scripts/emulation/run.py [--build] [--gdb] [elf]

Options:
  --build   Build (incrementally) before running
  --gdb     Start QEMU halted with a GDB server on tcp::3333
            (connect: arm-none-eabi-gdb <elf> -ex 'target remote :3333')
  elf       Path to an ELF to run (default: build/mps2_an386/emu_test/emu_test)

QEMU console = CMSDK UART0 -> stdio. Exit QEMU with Ctrl-A then X.
The default test app terminates QEMU by itself via semihosting.
"""

import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RTOS_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))

DEFAULT_ELF = os.path.join(RTOS_ROOT, "build", "mps2_an386", "emu_test",
                           "emu_test")


def main():
    parser = argparse.ArgumentParser(
        description="Run VibeRTOS in QEMU (ARM MPS2 AN386)")
    parser.add_argument("--build", action="store_true",
                        help="build (incrementally) before running")
    parser.add_argument("--gdb", action="store_true",
                        help="start QEMU halted with a GDB server on tcp::3333")
    parser.add_argument("elf", nargs="?", default=DEFAULT_ELF,
                        help="path to an ELF to run "
                             "(default: build/mps2_an386/emu_test/emu_test)")
    args = parser.parse_args()

    if shutil.which("qemu-system-arm") is None:
        print("ERROR: qemu-system-arm not found in PATH", file=sys.stderr)
        print("  macOS:  brew install qemu", file=sys.stderr)
        print("  Linux:  apt install qemu-system-arm", file=sys.stderr)
        return 1

    if args.build:
        rc = subprocess.run(
            [sys.executable, os.path.join(SCRIPT_DIR, "build.py")]
        ).returncode
        if rc != 0:
            return rc

    if not os.path.isfile(args.elf):
        print(f"ERROR: ELF not found: {args.elf}", file=sys.stderr)
        print("Build it first: ./scripts/emulation/build.py", file=sys.stderr)
        return 1

    qemu_args = [
        "qemu-system-arm",
        "-M", "mps2-an386",     # ARM MPS2, AN386 Cortex-M4 FPGA image
        "-cpu", "cortex-m4",
        "-nographic",           # CMSDK UART0 -> stdio (Ctrl-A X to quit)
        "-semihosting",         # allow the guest to exit QEMU (vibe_emu_exit)
        "-kernel", args.elf,
    ]

    if args.gdb:
        qemu_args += ["-S", "-gdb", "tcp::3333"]
        print("QEMU halted, GDB server on :3333")
        print(f"Connect with: arm-none-eabi-gdb {args.elf} "
              "-ex 'target remote :3333'")

    print(f"=== {' '.join(qemu_args)} ===")
    # Replace this process with QEMU so the console is fully interactive.
    os.execvp(qemu_args[0], qemu_args)


if __name__ == "__main__":
    sys.exit(main())
