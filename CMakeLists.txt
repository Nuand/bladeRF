cmake_minimum_required(VERSION 2.8)

# Indicate that this is a top-level build
set(BLADERF_TOP_LEVEL_BUILD ON)

#####################################################################
# Choose which major components to build
#####################################################################
option(ENABLE_HOST_BUILD
       "Build the bladeRF host components"
       ON
)

option(ENABLE_FX3_BUILD
       "Build the Cypress FX3 firmware component. Be sure to define FX3_SDK_PATH"
       OFF
)

#####################################################################
# Flag for development version vs tagged release
#####################################################################
option(TAGGED_RELEASE
       "Indicates that this is a tagged release.  Drops '-git' flag."
       OFF
)

if(TAGGED_RELEASE)
    set(VERSION_INFO_EXTRA "")
else()
    set(VERSION_INFO_EXTRA "git")
endif()

#####################################################################
# Build components
#####################################################################
if(ENABLE_HOST_BUILD)
    add_subdirectory(host)
else()
    message(STATUS "Skipping host component build")
endif()

if(ENABLE_FX3_BUILD)
    include(ExternalProject)
    ExternalProject_Add(fx3_firmware
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fx3_firmware"
        CMAKE_ARGS "-DFX3_INSTALL_PATH=${FX3_INSTALL_PATH};-DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fx3_firmware/cmake/fx3-toolchain.cmake"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/fx3_firmware"
        PREFIX     "${CMAKE_CURRENT_BINARY_DIR}/fx3_firmware"
        INSTALL_COMMAND ""
        BUILD_ALWAYS true
    )
else()
    message(STATUS "Skipping FX3 firmware build")
endif()
