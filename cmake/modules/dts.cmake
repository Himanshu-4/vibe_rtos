# dts.cmake
# CMake integration for Device Tree Source (DTS) processing.
# Compiles .dts to .dtb and generates a C header with device tree data.

cmake_minimum_required(VERSION 3.20)

# --- Paths ---
set(DTS_GENERATED_DIR ${CMAKE_BINARY_DIR}/include/generated)
set(DTS_HEADER        ${DTS_GENERATED_DIR}/device_tree_header.h)

# Ensure output directory exists
file(MAKE_DIRECTORY ${DTS_GENERATED_DIR})

# --- Find dtc (device tree compiler) ---
find_program(DTC_TOOL dtc DOC "Device tree compiler")

if(NOT DTC_TOOL)
    message(STATUS "DTS: dtc not found — device tree compilation will be skipped")
endif()

# --- Functions ---

# vibe_dts_compile(dts_file)
#
# Compiles a .dts file to a .dtb binary blob using dtc.
# Also generates a minimal C header with key device addresses.
function(vibe_dts_compile DTS_FILE)
    if(NOT EXISTS ${DTS_FILE})
        message(WARNING "DTS: file not found: ${DTS_FILE}")
        return()
    endif()

    get_filename_component(DTS_NAME ${DTS_FILE} NAME_WE)
    set(DTB_FILE ${CMAKE_BINARY_DIR}/${DTS_NAME}.dtb)

    if(DTC_TOOL)
        add_custom_command(
            OUTPUT  ${DTB_FILE}
            COMMAND ${DTC_TOOL} -I dts -O dtb -o ${DTB_FILE} ${DTS_FILE}
            DEPENDS ${DTS_FILE}
            COMMENT "Compiling device tree: ${DTS_NAME}.dts -> ${DTS_NAME}.dtb"
        )
        add_custom_target(dts_${DTS_NAME} ALL DEPENDS ${DTB_FILE})
        message(STATUS "DTS: will compile ${DTS_FILE}")
    else()
        message(STATUS "DTS: skipping dtc compilation (dtc not found)")
    endif()

    # Always generate the C header (from DTS directly in CMake as a fallback)
    vibe_dts_generate_header(${DTS_FILE} ${DTS_HEADER})
endfunction()

# vibe_dts_generate_header(dts_file output_header)
#
# Parses a simplified DTS file and generates a C header with
# register base addresses and configuration values.
# This is a lightweight fallback when dtc / full DTS tooling is unavailable.
function(vibe_dts_generate_header DTS_FILE OUTPUT_HEADER)
    file(STRINGS ${DTS_FILE} DTS_LINES)

    set(HDR "/* Auto-generated device tree header — do not edit */\n")
    set(HDR "${HDR}/* Source: ${DTS_FILE} */\n\n")
    set(HDR "${HDR}#ifndef VIBE_DEVICE_TREE_HEADER_H\n")
    set(HDR "${HDR}#define VIBE_DEVICE_TREE_HEADER_H\n\n")
    set(HDR "${HDR}#include <stdint.h>\n\n")

    # Extract reg = <addr size>; entries and create address macros
    set(CURRENT_NODE "")
    foreach(LINE ${DTS_LINES})
        # Detect node labels like "uart0: uart@40034000 {"
        if(LINE MATCHES "([a-z0-9_]+)@([0-9a-fA-F]+)[ \t]*\\{")
            string(TOUPPER "${CMAKE_MATCH_1}" NODE_UPPER)
            set(CURRENT_ADDR "0x${CMAKE_MATCH_2}")
            set(HDR "${HDR}#define DT_${NODE_UPPER}_BASE_ADDR ${CURRENT_ADDR}U\n")
        endif()

        # Detect clock-frequency = <value>;
        if(LINE MATCHES "clock-frequency = <([0-9]+)>")
            if(CURRENT_NODE)
                string(TOUPPER "${CURRENT_NODE}" N)
                set(HDR "${HDR}#define DT_${N}_CLOCK_FREQ ${CMAKE_MATCH_1}U\n")
            endif()
        endif()
    endforeach()

    set(HDR "${HDR}\n#endif /* VIBE_DEVICE_TREE_HEADER_H */\n")
    file(WRITE ${OUTPUT_HEADER} ${HDR})
    message(STATUS "DTS: generated header at ${OUTPUT_HEADER}")
endfunction()

# --- Auto-detect board DTS if VIBE_BOARD is set ---
if(DEFINED VIBE_BOARD AND DEFINED VIBE_BOARDS_DIR)
    set(_BOARD_DTS ${VIBE_BOARDS_DIR}/arm/${VIBE_BOARD}/${VIBE_BOARD}.dts)
    if(EXISTS ${_BOARD_DTS})
        vibe_dts_compile(${_BOARD_DTS})
    else()
        message(STATUS "DTS: no DTS file found for board '${VIBE_BOARD}' at ${_BOARD_DTS}")
        # Generate empty placeholder header
        file(WRITE ${DTS_HEADER}
"/* No DTS file found for this board */\n"
"#ifndef VIBE_DEVICE_TREE_HEADER_H\n"
"#define VIBE_DEVICE_TREE_HEADER_H\n"
"#endif\n")
    endif()
else()
    # Generate empty placeholder header
    file(WRITE ${DTS_HEADER}
"/* No board selected — DTS header placeholder */\n"
"#ifndef VIBE_DEVICE_TREE_HEADER_H\n"
"#define VIBE_DEVICE_TREE_HEADER_H\n"
"#endif\n")
endif()
