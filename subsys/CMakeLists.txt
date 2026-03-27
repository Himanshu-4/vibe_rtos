cmake_minimum_required(VERSION 3.20)

set(_SUBSYS_SRCS "")

if(CONFIG_LOG)
    list(APPEND _SUBSYS_SRCS logging/log.c)
endif()

if(CONFIG_SHELL)
    list(APPEND _SUBSYS_SRCS shell/shell.c)
endif()

if(CONFIG_COREDUMP)
    list(APPEND _SUBSYS_SRCS
        coredump/coredump.c
        coredump/coredump_arch_arm.c
    )
    if(CONFIG_COREDUMP_SHELL)
        list(APPEND _SUBSYS_SRCS coredump/coredump_shell.c)
    endif()
endif()

if(NOT _SUBSYS_SRCS)
    return()
endif()

add_library(vibe_subsys STATIC ${_SUBSYS_SRCS})

target_include_directories(vibe_subsys PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shell
    ${CMAKE_CURRENT_SOURCE_DIR}/logging
    ${VIBE_RTOS_ROOT}/include
    ${VIBE_RTOS_ROOT}/arch/arm/cortex_m/include
    ${CMAKE_BINARY_DIR}/include/generated
)
