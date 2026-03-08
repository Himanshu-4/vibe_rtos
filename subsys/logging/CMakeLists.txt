if(CONFIG_LOG)
    target_sources(vibe_subsys INTERFACE log.c)
    target_include_directories(vibe_subsys INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
