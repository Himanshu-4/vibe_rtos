if(CONFIG_WATCHDOG)
    target_sources(vibe_drivers INTERFACE watchdog.c)
endif()
