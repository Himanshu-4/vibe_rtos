if(CONFIG_GPIO)
    target_sources(vibe_drivers INTERFACE gpio.c)
    target_include_directories(vibe_drivers INTERFACE
        ${VIBE_RTOS_ROOT}/include/drivers
    )
endif()
