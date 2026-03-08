if(CONFIG_SHELL)
    target_sources(vibe_subsys INTERFACE shell.c)
    target_include_directories(vibe_subsys INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
