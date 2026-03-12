# VibeRTOS

A real-time operating system for ARM Cortex-M microcontrollers, inspired by Zephyr RTOS.

## Overview

VibeRTOS targets the Raspberry Pi Pico family and any ARM Cortex-M device (M0+, M33, M4, M55). It provides a preemptive, priority-based scheduler, static and dynamic memory allocation, a rich synchronisation API, a Kconfig-based configuration system, and an extensible driver model — all with a minimal footprint suitable for resource-constrained embedded systems.

## Features

- **Preemptive scheduler** — priority-based with round-robin at equal priorities; SMP-ready design
- **Tickless idle** — SysTick reprogrammed to next wakeup for lowest possible power consumption
- **Thread management** — create, suspend, resume, sleep, priority change, optional restart
- **Synchronisation** — mutex (with priority inheritance), semaphore, event flags, message queue, pipe
- **Memory** — static memory pools and a heap allocator; configurable pool sizes via Kconfig
- **Work queues** — deferred work items executed in a dedicated thread
- **Timers** — one-shot and periodic software timers
- **Coredump** — CPU state and stack snapshot saved to `.noinit` RAM on fault; survives warm reset
- **Logging** — compile-time level filtering, optional UART output
- **Shell** — interactive serial command line; commands registered at link time
- **Driver model** — uniform `vibe_device_t` API for GPIO, UART, SPI, I2C, Timer, ADC, Flash, RTC, Watchdog
- **Kconfig** — menuconfig / guiconfig / defconfig; generates `autoconf.h`, `sdkconfig.cmake`, `sdkconfig.json`
- **Device Tree** — DTS/DTSI hardware description (work in progress)
- **C11 kernel, C++17 application layer**

## Supported Boards

| Board | SoC | Core | Flash | RAM |
|-------|-----|------|-------|-----|
| `rpi_pico` | RP2040 | Cortex-M0+ × 2 | 2 MB | 264 KB |
| `rpi_pico2` | RP2350 | Cortex-M33 × 2 | 2 MB | 520 KB |

## Directory Structure

```
vibe_rtos/
├── arch/               # Architecture port (ARM Cortex-M)
│   └── arm/cortex_m/
│       ├── core/       # IRQ, SysTick, NVIC, context switch
│       └── include/
├── boards/             # Board-specific config and linker scripts
│   └── arm/
│       ├── rpi_pico/
│       └── rpi_pico2/
├── cmake/              # Build system helpers
│   ├── modules/        # kconfig.cmake, dts.cmake
│   ├── toolchains/     # arm-none-eabi.cmake
│   ├── vibe_rtos.cmake # vibe_add_application / vibe_set_board macros
│   └── vibe_rtos-config.cmake  # find_package() support
├── drivers/            # Peripheral drivers (GPIO, UART, SPI, I2C, …)
├── include/vibe/       # Public kernel headers
├── kernel/             # Scheduler, threads, timers, sync primitives
├── lib/                # Utility libraries (ring buffer, …)
├── samples/            # Example applications
│   ├── blinky/
│   ├── hello_world/
│   └── threads/
├── scripts/
│   └── kconfig/        # gen_configs.py — kconfiglib wrapper
├── soc/                # SoC-level definitions
├── subsys/             # Optional subsystems
│   ├── coredump/       # Fault capture and post-mortem dump
│   ├── logging/        # Log subsystem
│   └── shell/          # Interactive serial shell
└── tests/              # Unit and integration tests
```

## Quick Start

### Prerequisites

- `arm-none-eabi-gcc` 12.x or later
- CMake 3.20+
- Ninja
- Python 3.9+ (for Kconfig)

```sh
# Clone
git clone https://github.com/yourusername/vibe_rtos.git
cd vibe_rtos

# Create Python venv and install kconfiglib
python3 -m venv .venv
source .venv/bin/activate
pip install kconfiglib
```

### Build the blinky sample

```sh
cd samples/blinky
mkdir build && cd build

# Configure (defaults to rpi_pico board)
cmake .. -GNinja -DVIBE_BOARD=rpi_pico \
    -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/arm-none-eabi.cmake

# Build
ninja
```

Output: `blinky.elf`, `blinky.bin`, `blinky.hex`

### Flash to Raspberry Pi Pico

Hold BOOTSEL while connecting USB, then:

```sh
cp blinky.uf2 /Volumes/RPI-RP2       # macOS
# or
picotool load blinky.elf && picotool reboot
```

### Kconfig

```sh
ninja menuconfig    # ncurses TUI  (requires: pip install kconfiglib)
ninja guiconfig     # Tkinter GUI
ninja defconfig     # reset to board defaults
ninja saveconfig    # write back to board defconfig
```

### Out-of-tree Application

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app C ASM)

list(APPEND CMAKE_PREFIX_PATH /path/to/vibe_rtos)
find_package(vibe_rtos REQUIRED)

vibe_add_application(my_app)
vibe_set_board(rpi_pico)

target_sources(my_app PRIVATE main.c)
```

## Configuration

The build system generates four files in the build directory from `sdkconfig`:

| File | Purpose |
|------|---------|
| `sdkconfig` | Kconfig `.config` — source of truth |
| `include/generated/autoconf.h` | C `#define CONFIG_*` macros |
| `sdkconfig.cmake` | CMake `set(CONFIG_* ...)` variables |
| `sdkconfig.json` | JSON config snapshot |

Key options:

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_SYS_CLOCK_HZ` | 1000 | Tick rate in Hz |
| `CONFIG_MAX_THREADS` | 16 | Maximum thread count |
| `CONFIG_LOG` | n | Enable logging subsystem |
| `CONFIG_SHELL` | n | Enable interactive shell |
| `CONFIG_COREDUMP` | n | Enable fault coredump |
| `CONFIG_COREDUMP_STACK_SIZE` | 128 | Stack bytes saved per dump |
| `CONFIG_HEAP_MEM` | n | Enable dynamic heap allocator |

## Coredump

Enable with `CONFIG_COREDUMP=y`. On any fault (HardFault, MemManage, BusFault, UsageFault, assert, stack overflow), the full CPU register set, fault status registers, and a stack snapshot are written to a `.noinit` RAM region that survives a warm reset.

After reboot, the saved record can be inspected:

```c
// In application code
vibe_coredump_init();   // call early in main — prints notice if crash present
vibe_coredump_dump();   // print full decoded report
vibe_coredump_clear();  // erase the record
```

With `CONFIG_COREDUMP_SHELL=y` and `CONFIG_SHELL=y`:

```
vibe> coredump           # print full dump
vibe> coredump status    # one-line valid/invalid notice
vibe> coredump clear     # erase
```

## License

MIT — see [LICENSE](LICENSE).
