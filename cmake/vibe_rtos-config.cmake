# vibe_rtos-config.cmake
# find_package(vibe_rtos) support file.
#
# This file is installed to lib/cmake/vibe_rtos/ and is found automatically
# by CMake's find_package mechanism.
#
# Usage in an out-of-tree application:
#
#   list(APPEND CMAKE_PREFIX_PATH /path/to/vibe_rtos)
#   find_package(vibe_rtos REQUIRED)
#   vibe_add_application(my_app)

# Locate the VibeRTOS root relative to this config file
get_filename_component(_VIBE_CONFIG_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(VIBE_RTOS_ROOT   "${_VIBE_CONFIG_DIR}/../../.." ABSOLUTE)

# Include the main helper
include("${CMAKE_CURRENT_LIST_DIR}/vibe_rtos.cmake")

# Signal that the package was found
set(VIBE_RTOS_FOUND TRUE)
set(vibe_rtos_FOUND TRUE)

message(STATUS "Found VibeRTOS at ${VIBE_RTOS_ROOT}")
