#!/usr/bin/env python3
"""build.py — build the VibeRTOS QEMU emulation test app.

Usage:
  ./scripts/emulation/build.py [--clean] [--continue-boot] [-j N] [-v]

Options:
  --clean          Remove the build directory before building
  --continue-boot  Build with EMU_CONTINUE_BOOT (don't exit QEMU after tests)
  -j N             Parallel jobs (default: number of CPUs)
  -v               Verbose make output

Output: build/mps2_an386/emu_test/emu_test (ELF, loaded directly by QEMU)
"""

import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RTOS_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))

BOARD = "mps2_an386"
APP_DIR = os.path.join(SCRIPT_DIR, "app")
BUILD_DIR = os.path.join(RTOS_ROOT, "build", BOARD, "emu_test")
TOOLCHAIN = os.path.join(RTOS_ROOT, "cmake", "toolchains", "arm-none-eabi.cmake")


def main():
    parser = argparse.ArgumentParser(
        description="Build the VibeRTOS QEMU emulation test app")
    parser.add_argument("--clean", action="store_true",
                        help="remove the build directory before building")
    parser.add_argument("--continue-boot", action="store_true",
                        help="build with EMU_CONTINUE_BOOT "
                             "(don't exit QEMU after tests)")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4,
                        help="parallel jobs (default: number of CPUs)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="verbose make output")
    args = parser.parse_args()

    if shutil.which("arm-none-eabi-gcc") is None:
        print("ERROR: arm-none-eabi-gcc not found in PATH", file=sys.stderr)
        print("  macOS:  brew install --cask gcc-arm-embedded", file=sys.stderr)
        print("  Linux:  apt install gcc-arm-none-eabi", file=sys.stderr)
        return 1

    print("=== VibeRTOS emulation build ===")
    print(f"  Board:  {BOARD} (QEMU mps2-an386)")
    print(f"  App:    {APP_DIR}")
    print(f"  Output: {BUILD_DIR}")
    print()

    if args.clean and os.path.isdir(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(BUILD_DIR, exist_ok=True)

    continue_boot = "ON" if args.continue_boot else "OFF"
    cmake_cmd = [
        "cmake", "-S", APP_DIR, "-B", BUILD_DIR,
        f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}",
        f"-DCMAKE_PREFIX_PATH={RTOS_ROOT}",
        f"-DVIBE_BOARD={BOARD}",
        f"-DEMU_CONTINUE_BOOT={continue_boot}",
    ]

    make_cmd = ["make", f"-j{args.jobs}", "-C", BUILD_DIR]
    if args.verbose:
        make_cmd.append("VERBOSE=1")

    for cmd in (cmake_cmd, make_cmd):
        rc = subprocess.run(cmd).returncode
        if rc != 0:
            return rc

    print()
    print("=== Build complete ===")
    print(f"  ELF: {os.path.join(BUILD_DIR, 'emu_test')}")
    print("  Run: ./scripts/emulation/run.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
