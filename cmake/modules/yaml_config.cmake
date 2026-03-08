# yaml_config.cmake
# Reads a vibe_config.yml application config file and translates keys
# to Kconfig-style #define overrides in autoconf.h.
#
# Usage (in application CMakeLists.txt, after find_package):
#   vibe_load_yaml_config(${CMAKE_CURRENT_SOURCE_DIR}/vibe_config.yml)

cmake_minimum_required(VERSION 3.20)

# vibe_load_yaml_config(yaml_file)
#
# Parses a YAML config file with a simple line-by-line parser.
# Supports the vibe_config.yml schema documented in samples/.
function(vibe_load_yaml_config YAML_FILE)
    if(NOT EXISTS ${YAML_FILE})
        message(STATUS "yaml_config: no vibe_config.yml found at ${YAML_FILE}")
        return()
    endif()

    message(STATUS "yaml_config: loading ${YAML_FILE}")

    file(STRINGS ${YAML_FILE} YAML_LINES)

    set(CURRENT_SECTION "")
    set(CURRENT_SUBSECTION "")

    foreach(LINE ${YAML_LINES})
        # Skip blank lines and YAML comments
        if(LINE MATCHES "^[ \t]*#" OR LINE MATCHES "^[ \t]*$")
            continue()
        endif()

        # Top-level key: value (no indent)
        if(LINE MATCHES "^([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*:[ \t]*(.+)$")
            set(KEY   "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")
            _vibe_yaml_apply_top(${KEY} "${VALUE}")
            set(CURRENT_SECTION "${KEY}")
            set(CURRENT_SUBSECTION "")

        # Section header (no value)
        elseif(LINE MATCHES "^([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*:[ \t]*$")
            set(CURRENT_SECTION "${CMAKE_MATCH_1}")
            set(CURRENT_SUBSECTION "")

        # Indented key: value (2-space indent = subsection entry)
        elseif(LINE MATCHES "^  ([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*:[ \t]*(.+)$")
            set(KEY   "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")
            _vibe_yaml_apply_section(${CURRENT_SECTION} ${KEY} "${VALUE}")

        # Doubly indented key: value (4-space indent = nested)
        elseif(LINE MATCHES "^    ([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*:[ \t]*(.+)$")
            set(KEY   "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")
            _vibe_yaml_apply_nested(${CURRENT_SECTION} ${CURRENT_SUBSECTION} ${KEY} "${VALUE}")

        # Subsection header (2-space indent, no value)
        elseif(LINE MATCHES "^  ([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*:[ \t]*$")
            set(CURRENT_SUBSECTION "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    message(STATUS "yaml_config: done processing ${YAML_FILE}")
endfunction()

# Internal helper: top-level keys
function(_vibe_yaml_apply_top KEY VALUE)
    if(KEY STREQUAL "board")
        set(VIBE_BOARD "${VALUE}" CACHE STRING "Board from vibe_config.yml" FORCE)
        message(STATUS "yaml_config: board = ${VALUE}")
    endif()
endfunction()

# Internal helper: section-level keys
function(_vibe_yaml_apply_section SECTION KEY VALUE)
    string(TOUPPER "${SECTION}" S)
    string(TOUPPER "${KEY}"     K)

    # Convert yaml true/false to 1/0
    if(VALUE STREQUAL "true")
        set(VALUE "1")
    elseif(VALUE STREQUAL "false")
        set(VALUE "0")
    endif()

    if(SECTION STREQUAL "kernel")
        if(KEY STREQUAL "tick_hz")
            _vibe_yaml_set_config("CONFIG_SYS_CLOCK_HZ" "${VALUE}")
        elseif(KEY STREQUAL "heap_size")
            _vibe_yaml_set_config("CONFIG_HEAP_SIZE" "${VALUE}")
            _vibe_yaml_set_config("CONFIG_HEAP_MEM" "1")
        elseif(KEY STREQUAL "smp")
            _vibe_yaml_set_config("CONFIG_SMP" "${VALUE}")
        endif()

    elseif(SECTION STREQUAL "subsys")
        if(KEY STREQUAL "shell")
            _vibe_yaml_set_config("CONFIG_SHELL" "${VALUE}")
        endif()

    elseif(SECTION STREQUAL "drivers")
        if(KEY STREQUAL "uart")
            _vibe_yaml_set_config("CONFIG_UART" "${VALUE}")
        elseif(KEY STREQUAL "gpio")
            _vibe_yaml_set_config("CONFIG_GPIO" "${VALUE}")
        elseif(KEY STREQUAL "spi")
            _vibe_yaml_set_config("CONFIG_SPI" "${VALUE}")
        elseif(KEY STREQUAL "i2c")
            _vibe_yaml_set_config("CONFIG_I2C" "${VALUE}")
        endif()
    endif()
endfunction()

# Internal helper: nested keys (section -> subsection -> key)
function(_vibe_yaml_apply_nested SECTION SUBSECTION KEY VALUE)
    if(VALUE STREQUAL "true")
        set(VALUE "1")
    elseif(VALUE STREQUAL "false")
        set(VALUE "0")
    endif()

    if(SECTION STREQUAL "subsys" AND SUBSECTION STREQUAL "logging")
        if(KEY STREQUAL "enabled")
            _vibe_yaml_set_config("CONFIG_LOG" "${VALUE}")
        elseif(KEY STREQUAL "level")
            # Convert string level to integer
            if(VALUE STREQUAL "err")
                _vibe_yaml_set_config("CONFIG_LOG_DEFAULT_LEVEL" "0")
            elseif(VALUE STREQUAL "wrn")
                _vibe_yaml_set_config("CONFIG_LOG_DEFAULT_LEVEL" "1")
            elseif(VALUE STREQUAL "inf")
                _vibe_yaml_set_config("CONFIG_LOG_DEFAULT_LEVEL" "2")
            elseif(VALUE STREQUAL "dbg")
                _vibe_yaml_set_config("CONFIG_LOG_DEFAULT_LEVEL" "3")
            endif()
        elseif(KEY STREQUAL "backend")
            if(VALUE STREQUAL "uart")
                _vibe_yaml_set_config("CONFIG_LOG_BACKEND_UART" "1")
            elseif(VALUE STREQUAL "rtt")
                _vibe_yaml_set_config("CONFIG_LOG_BACKEND_RTT" "1")
            endif()
        endif()
    endif()
endfunction()

# _vibe_yaml_set_config(config_name value)
# Appends an override to the generated autoconf.h
function(_vibe_yaml_set_config CONFIG_NAME VALUE)
    set(AUTOCONF_FILE ${CMAKE_BINARY_DIR}/include/generated/autoconf.h)

    if(EXISTS ${AUTOCONF_FILE})
        # Remove any existing definition of this config
        file(STRINGS ${AUTOCONF_FILE} EXISTING_LINES)
        set(NEW_CONTENT "")
        foreach(L ${EXISTING_LINES})
            if(NOT L MATCHES "^#define ${CONFIG_NAME} " AND
               NOT L MATCHES "^/\\* ${CONFIG_NAME} is not set")
                set(NEW_CONTENT "${NEW_CONTENT}${L}\n")
            endif()
        endforeach()
        # Append new definition before endif
        string(REPLACE "#endif /* VIBE_AUTOCONF_H */"
            "#define ${CONFIG_NAME} ${VALUE}\n#endif /* VIBE_AUTOCONF_H */"
            NEW_CONTENT "${NEW_CONTENT}")
        file(WRITE ${AUTOCONF_FILE} "${NEW_CONTENT}")
    endif()

    message(STATUS "yaml_config: override ${CONFIG_NAME} = ${VALUE}")
endfunction()
