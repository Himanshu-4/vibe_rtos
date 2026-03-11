if(CONFIG_UART)
    target_sources(vibe_drivers PRIVATE uart.c)
    target_include_directories(vibe_drivers PUBLIC
        ${VIBE_RTOS_ROOT}/include/drivers
    )
endif()
