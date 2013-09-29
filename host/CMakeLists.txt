# Copyright 2013 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
#
# Portions of this file have been borrowed from and/or inspired by
# the CMakeLists.txt from the rtl-sdr project.
#   http://sdr.osmocom.org/trac/wiki/rtl-sdr

cmake_minimum_required(VERSION 2.8.3)
project(bladeRF C)

################################################################################
# Project configuration
################################################################################

# All build output lands in this directory:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to a release build.")
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Build type")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

# Firmware <--> Host common files
set(BLADERF_FW_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware_common)

# Source and headers common amongst host software
set(BLADERF_HOST_COMMON_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/common/include
        ${CMAKE_CURRENT_BINARY_DIR}/common/include
)

set(BLADERF_HOST_COMMON_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/common/src)

###############################
# Misc. options
###############################
option(ENABLE_LOG_FILE_INFO
        "Enable source file and line number information in log messages."
        OFF)

if(ENABLE_LOG_FILE_INFO)
    add_definitions(-DLOG_INCLUDE_FILE_INFO)
endif()

################################################################################
# Configure host-specific items
################################################################################
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include/host_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/common/include/host_config.h
    @ONLY
)

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
    add_definitions(-Werror)
    add_definitions(-Wno-unused-parameter)

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_definitions(-DDEBUG=1)
        add_definitions(-O0)
    endif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

endif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
      "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

###############################
# MS Visual Studio
###############################
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
endif(MSVC)

################################################################################
# System configuration
################################################################################

# Linux (TODO determine if this will actually be sufficient for BSD)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    set(BLADERF_OS_LINUX 1)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(BLADERF_OS_OSX 1)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(BLADERF_OS_WINDOWS 1)

    # We also need to add some headers that the system will not provide
    set(BLADERF_HOST_COMMON_INCLUDE_DIRS
            ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_LIST_DIR}/common/include/windows
    )

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    message(FATAL_ERROR "Unsupported host operating system: ${CMAKE_SYSTEM_NAME}")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include/host_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/common/include/host_config.h
    @ONLY
)

################################################################################
# Process subdirectories
################################################################################
add_subdirectory(libraries)
add_subdirectory(misc)
add_subdirectory(utilities)

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

################################################################################
# Windows-only, Post build: Copy libraries to our build output directory
################################################################################
if(WIN32)
    find_package(LibPThreadsWin32)
    if(LIBPTHREADSWIN32_FOUND)
        set(WIN_POSTBUILD_ITEMS ${WIN_POSTBUILD_ITEMS}
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/pthreadVC2.dll")

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/pthreadVC2.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBPTHREADSWIN32_PATH}/dll/${LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX}/pthreadVC2.dll"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/"
            COMMENT "Copying pthreadVC2.dll to output directory..."
        )
    endif(LIBPTHREADSWIN32_FOUND)

    find_package(LibUsb)
    if(LIBUSB_FOUND)
        set(WIN_POSTBUILD_ITEMS ${WIN_POSTBUILD_ITEMS}
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/libusb-1.0.dll")

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/libusb-1.0.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBUSB_PATH}/${LIBUSB_LIBRARY_PATH_SUFFIX}/libusb-1.0.dll"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/"
            COMMENT "Copying libusb-1.0.dll to output directory..."
        )
    endif(LIBUSB_FOUND)

    add_custom_target(win_postbuild ALL DEPENDS ${WIN_POSTBUILD_ITEMS})

endif(WIN32)
