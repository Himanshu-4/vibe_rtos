if(CONFIG_UART)
    target_sources(vibe_drivers INTERFACE uart.c)
    target_include_directories(vibe_drivers INTERFACE
        ${VIBE_RTOS_ROOT}/include/drivers
    )
endif()
