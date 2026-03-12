# subsys/coredump/CMakeLists.txt
# Sources are appended to the parent vibe_subsys target.

if(CONFIG_COREDUMP)
    target_sources(vibe_subsys PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/coredump.c
        ${CMAKE_CURRENT_SOURCE_DIR}/coredump_arch_arm.c
        ${CMAKE_CURRENT_SOURCE_DIR}/coredump_shell.c
    )
endif()
