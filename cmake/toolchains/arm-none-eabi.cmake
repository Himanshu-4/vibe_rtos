# arm-none-eabi.cmake
# CMake toolchain file for ARM bare-metal GCC (arm-none-eabi).
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Find tools — search common install paths
find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM GCC C compiler")

find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM GCC C++ compiler")

find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM GCC assembler (via gcc frontend)")

find_program(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM objcopy")

find_program(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM objdump")

find_program(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size
    PATHS /usr/bin /usr/local/bin /opt/arm-none-eabi/bin
    DOC "ARM size utility")

# Tell CMake not to run compiler checks against the host system.
# Cross-compilers always fail the default link test (no OS, no libc).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Skip compiler working / ABI / feature detection — we know it works.
set(CMAKE_C_COMPILER_WORKS   1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")
set(CMAKE_ASM_COMPILER_WORKS 1 CACHE INTERNAL "")

# --- Common compile flags ---
set(VIBE_ARCH_FLAGS
    "-mthumb"
    "-mabi=aapcs"
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-common"
    "-fno-exceptions"
)

# Bare-metal linker flags.
# -nostartfiles : do not link OS startup files (we provide our own Reset_Handler)
# -ffreestanding: freestanding environment (no OS assumptions)
# -lgcc         : GCC runtime helpers (__aeabi_uidiv, __clzsi2, etc.)
# -lc           : newlib-nano libc (memset, strncpy, etc.)
# NOTE: -nostdlib is intentionally omitted so that -lgcc/-lc are resolvable.
set(VIBE_NOLIB_FLAGS
    "-nostartfiles"
    "-ffreestanding"
)

# Debug flags
set(VIBE_DEBUG_FLAGS "-g3 -gdwarf-4")

# Join flags into space-separated strings for CMAKE_*_FLAGS_INIT
# (these variables are strings, not lists)
string(JOIN " " VIBE_C_FLAGS_STR  ${VIBE_ARCH_FLAGS})
string(JOIN " " VIBE_LD_FLAGS_STR ${VIBE_NOLIB_FLAGS} "-Wl,--gc-sections" "-Wl,-Map=output.map" "-lgcc" "-lc")

set(CMAKE_C_FLAGS_INIT           "${VIBE_C_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_INIT         "${VIBE_C_FLAGS_STR} -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT         "${VIBE_C_FLAGS_STR}")
set(CMAKE_EXE_LINKER_FLAGS_INIT  "${VIBE_LD_FLAGS_STR}")

# --- Cortex-M variant flags (set per-board or per-SoC) ---
# Override VIBE_CPU_FLAGS in your board CMakeLists.txt, e.g.:
#   set(VIBE_CPU_FLAGS "-mcpu=cortex-m0plus -mfloat-abi=soft")
# or:
#   set(VIBE_CPU_FLAGS "-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard")

if(NOT DEFINED VIBE_CPU_FLAGS)
    # Default: Cortex-M0+ (Raspberry Pi Pico)
    # Store as a CMake list (semicolon-separated) so each flag is a separate argument
    set(VIBE_CPU_FLAGS "-mcpu=cortex-m0plus;-mfloat-abi=soft")
    message(STATUS "VIBE_CPU_FLAGS not set — defaulting to Cortex-M0+")
endif()

# Split string flags into a proper CMake list before passing to compiler
separate_arguments(VIBE_CPU_FLAGS_LIST UNIX_COMMAND "${VIBE_CPU_FLAGS}")

add_compile_options(${VIBE_CPU_FLAGS_LIST})
add_link_options(${VIBE_CPU_FLAGS_LIST})

# Helper to generate .bin and .hex from ELF
function(vibe_generate_binary TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
        COMMAND ${CMAKE_SIZE}    $<TARGET_FILE:${TARGET}>
        COMMENT "Generating ${TARGET}.bin and ${TARGET}.hex"
    )
endfunction()
