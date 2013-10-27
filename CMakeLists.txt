cmake_minimum_required(VERSION 2.8)

#####################################################################
# Choose which major components to build
#####################################################################
option(ENABLE_HOST_BUILD
       "Build the bladeRF host components"
       ON
)

option(ENABLE_FX3_BUILD
       "Build the Cypress FX3 firmware component"
       ON
)

#####################################################################
# Build components
#####################################################################
if(ENABLE_HOST_BUILD)
    add_subdirectory(host)
else()
    message(STATUS "Skipping host component build")
endif()

if(ENABLE_FX3_BUILD)
    add_subdirectory(fx3_firmware)
else()
    message(STATUS "Skipping FX3 firmware build")
endif()
