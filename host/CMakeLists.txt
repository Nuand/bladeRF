# Copyright 2012 Nuand, LLC.
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

set(BLADERF_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../common)

################################################################################
# Compiler configuration
################################################################################

if("${CMAKE_CC_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_CC_COMPILER_ID}" STREQUAL "Clang" )
    ADD_DEFINITIONS(-Wall)
    ADD_DEFINITIONS(-Wextra)
    ADD_DEFINITIONS(-Wno-unused-parameter)
endif()


################################################################################
# Process subdirectories 
################################################################################

add_subdirectory(libraries)
#add_subdirectory(utilities)

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
