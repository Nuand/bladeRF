# Copyright 2013 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
cmake_minimum_required(VERSION 2.8)
project(bladeRF-flash C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  4)
set(VERSION_INFO_PATCH  0)
set(VERSION_INFO_EXTRA "git")
include(Version)

set(VERSION "${VERSION_INFO}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY
)

################################################################################
# Build dependencies
################################################################################
if(ENABLE_BACKEND_LIBUSB)
    find_package(LibUSB)
    if(NOT LIBUSB_FOUND)
        message(FATAL_ERROR "libusb-1.0 required to use for libusb backend")
    endif(NOT LIBUSB_FOUND)
else()
    return()
endif(ENABLE_BACKEND_LIBUSB)

################################################################################
# Include paths
################################################################################
set(FLASH_INCLUDE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}/src
       ${CMAKE_CURRENT_BINARY_DIR}/src
       ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
       ${libbladeRF_SOURCE_DIR}/include
       ${BLADERF_FW_COMMON_INCLUDE_DIR}
       ${LIBUSB_INCLUDE_DIRS})

include_directories(${FLASH_INCLUDE_DIRS})

################################################################################
# Configure source files
################################################################################
set(BLADERF_FLASH_SOURCE
        src/main.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/bladerf_devinfo.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/device_identifier.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/ezusb.c
)

if(MSVC)
    set(BLADERF_FLASH_SOURCE ${BLADERF_FLASH_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt.c
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
       )
endif()

add_executable(bladeRF-flash ${BLADERF_FLASH_SOURCE})

################################################################################
# Build configuration
################################################################################
set(FLASH_LINK_LIBRARIES
    libbladerf_shared
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(bladeRF-flash ${FLASH_LINK_LIBRARIES})

################################################################################
# Installation
################################################################################
if(NOT DEFINED BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR bin)
endif()

install(TARGETS bladeRF-flash DESTINATION ${BIN_INSTALL_DIR})

################################################################################
# Informational output
################################################################################
message(STATUS "Configured to build bladeRF-flash version: ${VERSION_INFO}")
