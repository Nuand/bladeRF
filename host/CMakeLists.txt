# Copyright 2013 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
#
# Portions of this file have been borrowed from and/or inspired by
# the CMakeLists.txt from the rtl-sdr project.
#   http://sdr.osmocom.org/trac/wiki/rtl-sdr

cmake_minimum_required(VERSION 2.8)

################################################################################
# Project configuration
################################################################################

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to a release build.")
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Build type")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

# Firmware <--> Host common files
set(BLADERF_FW_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware_common)

# Source common amongst host software
set(BLADERF_HOST_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/common)

################################################################################
# Compiler configuration
################################################################################

###############################
# GCC
###############################
option(ENABLE_GDB_EXTENSIONS
       "When performing a debug build with GCC, this will enable -ggdb instead of -g"
       OFF)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND
   "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

    # Personal preference - as much debug info as possible with GDB
    if(${ENABLE_GDB_EXTENSIONS})
        string(REGEX REPLACE "^-g$" "-ggdb3" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    endif(${ENABLE_GDB_EXTENSIONS})

endif()

###############################
# GCC and Clang
###############################
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
   "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

    # Prefer more warnings, enforce C89 so folks developing in Linux can keep
    # an eye on stuff that will likely cause portability issues
    add_definitions(-Wall)
    add_definitions(-Wextra)
# TODO Re-enable these
#    add_definitions(-pedantic)
#    add_definitions(-std=c89)
    add_definitions(-Wno-unused-parameter)

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_definitions(-DDEBUG=1)
        add_definitions(-O0)
    endif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

endif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
      "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")


################################################################################
# Process subdirectories
################################################################################

add_subdirectory(libraries)
add_subdirectory(utilities)
add_subdirectory(misc)

################################################################################
# Create uninstall target
################################################################################
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
