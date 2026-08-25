# VibeRTOS QEMU Emulation Environment

Run and test VibeRTOS on your desktop — no Raspberry Pi Pico required.
This directory contains everything needed to build a VibeRTOS image, boot it
under QEMU, and run an automated kernel self-test suite with pass/fail
reporting suitable for CI.

```
scripts/emulation/
├── README.md    # this file
├── build.py     # build the emulation test app
├── run.py       # boot it in QEMU (interactive, optional GDB)
├── test.py      # automated build + boot + verify (CI entry point)
└── app/         # the emulation test application
    ├── CMakeLists.txt
    ├── main.c            # kernel self-test suite
    └── vibe_config.yml
```

## Quick start

```sh
brew install qemu                      # macOS (Linux: apt install qemu-system-arm)
./scripts/emulation/test.py            # build + run + verify → "TEST RESULT: PASS"
./scripts/emulation/run.py --build     # interactive boot (Ctrl-A then X quits QEMU)
```

---

## Emulated hardware architecture

QEMU has **no RP2040/RP2350 machine model**, so emulation uses the same
approach as Zephyr and other RTOSes: the **ARM MPS2** FPGA prototyping board
with the **AN386** image — a **Cortex-M4**, which is one of the CPU variants
VibeRTOS supports (`CONFIG_CPU_CORTEX_M4`). Everything above the SoC layer
(kernel, scheduler, heap, ring buffer, subsystems, drivers model) is
identical to what runs on the Pico, so the emulator exercises the real
kernel code paths.

| Property      | Value                                              |
|---------------|----------------------------------------------------|
| QEMU machine  | `mps2-an386`                                       |
| CPU           | ARM Cortex-M4 (ARMv7-M), single core               |
| Clock         | 25 MHz fixed (no PLL setup needed)                 |
| FPU           | present in HW; VibeRTOS builds **soft-float**      |
| Console       | CMSDK APB UART0 → QEMU stdio                       |
| Test exit     | ARM semihosting `SYS_EXIT` (`-semihosting`)        |

### Memory map

| Region        | Base         | Size  | Use                                    |
|---------------|--------------|-------|----------------------------------------|
| ZBT SSRAM1    | `0x00000000` | 4 MB  | Code ("flash"): vector table, .text    |
| ZBT SSRAM2/3  | `0x20000000` | 4 MB  | Data: .data, .bss, .noinit, heap, stack|
| PSRAM         | `0x21000000` | 16 MB | Unused (available for future tests)    |
| CMSDK APB     | `0x40000000` | —     | Timers, UARTs, watchdog                |
| FPGA IO / SCC | `0x40028000` | —     | LEDs, buttons (unused)                 |
| ARMv7-M PPB   | `0xE000E000` | —     | SysTick, NVIC, SCB (standard)          |

The Cortex-M4 fetches its initial SP/PC from address `0x0` at reset, so the
vector table lives at the start of ZBT SSRAM1. There is no real flash: QEMU's
`-kernel` loads the ELF segments directly into the SRAMs, but `.data` still
uses the standard LMA/VMA copy scheme and is initialised by `Reset_Handler`
exactly as on real hardware.

### Key peripherals

- **CMSDK APB UART0** (`0x40004000`) — registers `DATA/STATE/CTRL/INTCLEAR/BAUDDIV`.
  The SoC support routes `vibe_printk` output here via the `vibe_console_putc()`
  hook; QEMU connects it to stdio. IRQs 0 (RX) / 1 (TX) are wired but the
  console currently uses polling.
- **SysTick** — standard ARMv7-M, driven from the 25 MHz CPU clock;
  `arch_systick_init(CONFIG_SYS_CLOCK_HZ)` works unmodified.
- **Semihosting** — `vibe_emu_exit(code)` (see `soc/arm/arm/mps2_an386/soc.c`)
  executes `BKPT #0xAB` with the `SYS_EXIT` reason code, terminating the QEMU
  process with exit status 0 (pass) or 1 (fail). Only valid under
  `qemu-system-arm -semihosting`; on real hardware it would HardFault.

### Port files (outside this directory)

| File | Purpose |
|------|---------|
| `boards/arm/mps2_an386/` | board: CMakeLists, defconfig, `linker.ld`, Kconfig, DTS |
| `soc/arm/arm/mps2_an386/` | SoC: `soc.h` (memory map/regs), `soc.c` (`SystemInit`, console putc, semihosting exit) |
| `cmake/toolchains/arm-none-eabi.cmake` | `mps2_an386` → `-mcpu=cortex-m4 -mfloat-abi=soft` |
| `include/vibe/sys/printk.h`, `kernel/init.c` | weak `vibe_console_putc()` hook the SoC overrides |

`soc.c` is linked **directly into the application** (via `VIBE_SOC_SOURCES`),
not archived into a static library — otherwise the linker could satisfy
`SystemInit`/`vibe_console_putc` from their weak defaults and silently drop
the strong overrides.

---

## Prerequisites

- `qemu-system-arm` — `brew install qemu` / `apt install qemu-system-arm`
- `arm-none-eabi-gcc` — `brew install --cask gcc-arm-embedded` / `apt install gcc-arm-none-eabi`
- Python venv with `kconfiglib` at `.venv/` (already set up for this repo;
  otherwise: `python3 -m venv .venv && .venv/bin/pip install kconfiglib`)

## Build

```sh
./scripts/emulation/build.py                  # incremental
./scripts/emulation/build.py --clean          # from scratch
./scripts/emulation/build.py --continue-boot  # don't exit after tests; keep booting
```

Output: `build/mps2_an386/emu_test/emu_test` (ELF — QEMU loads this directly;
`.bin`/`.hex` are also generated).

Manual equivalent:

```sh
cmake -S scripts/emulation/app -B build/mps2_an386/emu_test \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
      -DCMAKE_PREFIX_PATH=$PWD -DVIBE_BOARD=mps2_an386
make -C build/mps2_an386/emu_test
```

## Run

```sh
./scripts/emulation/run.py            # boot the last build
./scripts/emulation/run.py --build    # build first, then boot
./scripts/emulation/run.py path/to/other.elf
```

Under the hood:

```sh
qemu-system-arm -M mps2-an386 -cpu cortex-m4 -nographic -semihosting -kernel emu_test
```

- Console I/O is on your terminal (UART0 ↔ stdio).
- Quit QEMU with **Ctrl-A, then X** (the default test app exits by itself).

### Debugging with GDB

```sh
./scripts/emulation/run.py --gdb      # QEMU starts halted, gdbserver on :3333
# in another terminal:
arm-none-eabi-gdb build/mps2_an386/emu_test/emu_test -ex 'target remote :3333'
(gdb) break vibe_init
(gdb) continue
```

This is the recommended way to develop the unimplemented kernel pieces
(e.g. single-step through `PendSV_Handler` while implementing the context
switch).

## Test

```sh
./scripts/emulation/test.py                    # build + run + verify
./scripts/emulation/test.py --no-build         # reuse existing ELF
./scripts/emulation/test.py --timeout 60       # generous timeout (default 30s)
```

`test.py` passes only if **both** hold:

1. the guest prints `*** EMULATION TESTS PASSED ***` on the console, and
2. QEMU exits with status 0 (set by the guest through semihosting).

The full console log is saved to `build/mps2_an386/emu_test/qemu_test.log`.
Exit status is 0/1, so CI integration is just:

```yaml
- run: ./scripts/emulation/test.py
```

### What the test app covers

The suite (`app/main.c`) runs in two phases:

**Phase 1 — pre-scheduler** (an `APPLICATION`-level device init inside
`vibe_init()`):

- `vibe_printk` / `vibe_snprintk` formatting (`%d %u %x %s %c %p %%` + `l`
  modifier, buffer truncation)
- heap allocator: alloc / calloc-zeroing / realloc / free / stats accounting
- ring buffer: init, put/get round-trip, occupancy tracking
- device registry: per-level init ordering, lookup, negative lookup
- RTT: SEGGER-compatible control block, up-channel write/occupancy,
  down-channel read
- trace API error handling

**Phase 2 — scheduler** (real threads created in `main()` before
`vibe_init()`):

- first context switch (PendSV) out of `vibe_sched_start()`
- SysTick-driven preemption and `vibe_thread_sleep()` wake-ups
- round-robin time-slicing between two equal-priority CPU-bound spinners
- thread-exit trampoline (entry function returning → thread `DEAD`)
- kernel trace hooks (`vibe_trace_tick`, `vibe_trace_thread_switch_in`
  strongly overridden by the app — the same mechanism a trace library uses)
- per-thread TCB trace fields (`switch_in_count`, `total_runtime`)
- `VIBE_LOG_*` varargs formatting

Extend it by adding `test_*()` functions; use `check(condition, "description")`
for each assertion.

## Feature notes

- **Context switching is live**: `PendSV_Handler`
  (`arch/arm/cortex_m/core/context_switch.S`, Thumb-1 — runs on M0+ through
  M33) plus `arch_switch_to()` in `core/irq.c`. PendSV tracks whose
  registers are physically on the CPU, so coalesced or re-pended switches
  degenerate into harmless identity switches.
- **RTT** (`subsys/rtt`, `CONFIG_RTT=y`): SEGGER-compatible `_SEGGER_RTT`
  control block. On real hardware attach with J-Link RTT Viewer, OpenOCD
  (`rtt setup/start`) or pyOCD; `CONFIG_LOG_BACKEND_RTT=y` routes log lines
  to it. (QEMU has no SWD probe, so in emulation RTT is exercised via its
  in-memory API.)
- **Tracing** (`CONFIG_TRACE=y`): hook points for a future
  SystemView/Tracealyzer-style library (`include/vibe/trace.h`) and runtime
  stats fields in each TCB. Override any `vibe_trace_*` weak symbol to
  record events.

With `--continue-boot` the demo keeps running after the suite and prints a
heartbeat (`alive, uptime=...`) every second — handy for interactive
debugging with `run.py --gdb`.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `qemu-system-arm: unsupported machine` | QEMU too old — need a version with `mps2-an386` (≥ 2.10; any current install is fine) |
| No console output at all | Image built for another board? Rebuild with `build.py` (board `mps2_an386`) |
| `TEST RESULT: FAIL (timeout)` | Guest hung before the suite — run `run.py --gdb` and inspect |
| QEMU never exits in interactive run | Expected with `--continue-boot`; quit with Ctrl-A X |
| Semihosting exit ignored | `-semihosting` flag missing (run.py/test.py always pass it) |
