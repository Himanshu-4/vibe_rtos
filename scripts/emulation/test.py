#!/usr/bin/env python3
"""test.py — automated VibeRTOS emulation test run.

Builds the emulation test app, boots it in QEMU, and evaluates the result:
  1. The guest prints "*** EMULATION TESTS PASSED ***" on the console, and
  2. QEMU exits with status 0 (semihosting exit code from vibe_emu_exit).

Usage:
  ./scripts/emulation/test.py [--no-build] [--timeout <seconds>]

Exit status: 0 = tests passed, 1 = tests failed / timeout / boot error.
The full console log is kept in build/mps2_an386/emu_test/qemu_test.log.
"""

import argparse
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RTOS_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))

BUILD_DIR = os.path.join(RTOS_ROOT, "build", "mps2_an386", "emu_test")
ELF = os.path.join(BUILD_DIR, "emu_test")
LOG = os.path.join(BUILD_DIR, "qemu_test.log")

PASS_MARKER = "*** EMULATION TESTS PASSED ***"
FAIL_MARKER = "*** EMULATION TESTS FAILED ***"

TIMEOUT_STATUS = 124  # mirrors coreutils `timeout`


def fail(reason):
    print(f"TEST RESULT: FAIL ({reason})")
    return 1


def main():
    parser = argparse.ArgumentParser(
        description="Automated VibeRTOS emulation test run")
    parser.add_argument("--no-build", action="store_true",
                        help="reuse the existing ELF instead of building")
    parser.add_argument("--timeout", type=int, default=30,
                        help="QEMU run timeout in seconds (default: 30)")
    args = parser.parse_args()

    # -------------------------------------------------------------------
    # Build
    # -------------------------------------------------------------------
    if not args.no_build:
        rc = subprocess.run(
            [sys.executable, os.path.join(SCRIPT_DIR, "build.py")]
        ).returncode
        if rc != 0:
            return fail("build error")

    if not os.path.isfile(ELF):
        return fail(f"ELF missing: {ELF}")

    # -------------------------------------------------------------------
    # Run QEMU with a timeout
    # -------------------------------------------------------------------
    os.makedirs(BUILD_DIR, exist_ok=True)

    print(f"=== Running QEMU (timeout: {args.timeout}s) ===")
    with open(LOG, "wb") as log_file:
        proc = subprocess.Popen(
            ["qemu-system-arm",
             "-M", "mps2-an386", "-cpu", "cortex-m4",
             "-nographic", "-semihosting",
             "-kernel", ELF],
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            qemu_status = proc.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            print(f"ERROR: QEMU did not exit within {args.timeout}s — "
                  "killing it", file=sys.stderr)
            proc.kill()
            proc.wait()
            qemu_status = TIMEOUT_STATUS

    # -------------------------------------------------------------------
    # Evaluate
    # -------------------------------------------------------------------
    with open(LOG, "r", errors="replace") as f:
        log_text = f.read()

    print()
    print("=== Console output ===")
    print(log_text, end="")
    print("======================")
    print(f"QEMU exit status: {qemu_status}")

    result = 0
    if PASS_MARKER not in log_text:
        if FAIL_MARKER in log_text:
            print("FAIL: guest reported test failures")
        elif qemu_status == TIMEOUT_STATUS:
            print("FAIL: timeout — guest never reached the test suite")
        else:
            print("FAIL: pass marker not found in console output")
        result = 1

    if qemu_status != 0:
        print(f"FAIL: QEMU exit status {qemu_status} "
              "(expected 0 from semihosting exit)")
        result = 1

    print()
    if result == 0:
        print("TEST RESULT: PASS")
    else:
        print(f"TEST RESULT: FAIL (log: {LOG})")
    return result


if __name__ == "__main__":
    sys.exit(main())
