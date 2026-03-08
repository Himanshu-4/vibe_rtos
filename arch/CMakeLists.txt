cmake_minimum_required(VERSION 3.20)

# Select the architecture subdirectory based on Kconfig.
# Currently only ARM is supported; extend by adding elseif branches.

if(CONFIG_ARCH_ARM OR NOT DEFINED CONFIG_ARCH_ARM)
    # Default to ARM if nothing is explicitly set
    add_subdirectory(arm)
else()
    message(FATAL_ERROR "Unsupported architecture. Set CONFIG_ARCH_ARM=y.")
endif()
