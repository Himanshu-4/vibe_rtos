if(CONFIG_GPIO)
    target_sources(vibe_drivers PRIVATE gpio.c)
    target_include_directories(vibe_drivers PUBLIC
        ${VIBE_RTOS_ROOT}/include/drivers
    )
endif()
