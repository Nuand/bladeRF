cmake_minimum_required(VERSION 3.5)
project(bladeRF C)

################################################################################
# Project configuration
################################################################################

# All build output lands in this directory:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)
set(CMAKE_HELPERS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/cmake/helpers)
set(CMAKE_HELPERS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/helpers)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to a release build.")
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Build type")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

# Firmware <--> Host common files
set(BLADERF_FW_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware_common)

# FPGA NIOS II <--> Host common files
set(BLADERF_FPGA_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../fpga_common)
set(BLADERF_FPGA_COMMON_INCLUDE_DIR ${BLADERF_FPGA_COMMON_DIR}/include)
set(BLADERF_FPGA_COMMON_SOURCE_DIR ${BLADERF_FPGA_COMMON_DIR}/src)

# Source and headers common amongst host software
set(BLADERF_HOST_COMMON_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/common/include
        ${CMAKE_CURRENT_BINARY_DIR}/common/include
)

set(BLADERF_HOST_COMMON_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/common/src)

# CMake >= 2.8.4 no longer defines WIN32. Acknowledge this change.
set(CMAKE_LEGACY_CYGWIN_WIN32 0)

###############################
# Misc. options
###############################
option(ENABLE_LOG_FILE_INFO
        "Enable source file and line number information in log messages."
        ON)

option(BUILD_DOCUMENTATION
        "Enable build and install of manpages and other documentation."
        OFF)

option(TREAT_WARNINGS_AS_ERRORS
       "Treat compiler warnings as errors."
       ON)

if(ENABLE_LOG_FILE_INFO)
    add_definitions(-DLOG_INCLUDE_FILE_INFO)
endif()

################################################################################
# Configure host-specific items
################################################################################
include(TestBigEndian)
TEST_BIG_ENDIAN(BLADERF_BIG_ENDIAN)

# Search for libraries and drivers
include(FindLibUSB)
if(WIN32)
    include(FindCyAPI)
endif()

include(CheckLibraryExists)
check_library_exists(c clock_gettime "time.h" HAVE_CLOCK_GETTIME)

################################################################################
# Build third-party libraries
################################################################################
# TODO: the build will fail unless this is enabled.
option(BUILD_AD936X
       "Include support for the Analog Devices AD936x RFIC."
       ON)
option(BUILD_AD936X_DEBUG
       "Enable debugging output for the AD936x library."
       OFF)
 # We do this prior to the rest of host/common, as the compile options are
# going to be different.
add_subdirectory(common/thirdparty/ad936x)

################################################################################
# Compiler configuration
################################################################################

###############################
# GCC and Clang
###############################
option(ENABLE_GDB_EXTENSIONS "Build with -ggdb3 instead of -g" OFF)

if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
   ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")

    # Personal preference - as much debug info as possible with GDB
    if(${ENABLE_GDB_EXTENSIONS})
        string(REGEX REPLACE "^-g$" "-ggdb3" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    endif(${ENABLE_GDB_EXTENSIONS})


    add_definitions(-Wall)
    add_definitions(-Wextra)
    add_definitions(-Wno-unused-parameter)

    if(TREAT_WARNINGS_AS_ERRORS)
        add_definitions(-Werror)
    endif()

    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        add_definitions(-DDEBUG=1)
        add_definitions(-O0)
    endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

endif(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
      ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")

###############################
# MS Visual Studio
###############################
if(MSVC)
    if(TREAT_WARNINGS_AS_ERRORS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
    endif()

    # Prior to V12.0 2013, certain C99 headers we use were not supported.
    # We have to supply our own.
    if(MSVC_VERSION LESS 1800)
        set(MSVC_C99_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/common/include/windows/c99)
    endif()

    # MSVC ships with its own timespec
    add_definitions(-DHAVE_STRUCT_TIMESPEC)
endif(MSVC)

################################################################################
# System configuration
################################################################################

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR CYGWIN)
    set(BLADERF_OS_LINUX 1)

    if(NOT CMAKE_CROSSCOMPILING AND NOT CYGWIN)
        message(STATUS "Checking libc version...")
        try_run(LIBC_VERSION_RUN_RESULT LIBC_VERSION_COMPILE_RESULT
                ${CMAKE_HELPERS_BINARY_DIR}
                ${CMAKE_HELPERS_SOURCE_DIR}/libc_version.c
                RUN_OUTPUT_VARIABLE LIBC_VERSION)

        if(LIBC_VERSION)
            message(STATUS "libc version: ${LIBC_VERSION}")
        else()
            message(WARNING "Unable to determine libc version")
        endif()
    endif(NOT CMAKE_CROSSCOMPILING AND NOT CYGWIN)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(BLADERF_OS_FREEBSD 1)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(BLADERF_OS_OSX 1)

    # We also need to add some headers that the system will not provide
    set(BLADERF_HOST_COMMON_INCLUDE_DIRS
            ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
            ${CMAKE_CURRENT_LIST_DIR}/common/include/osx
    )

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

# OSX
if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    if(${isSystemDir} STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

################################################################################
# Process subdirectories
################################################################################
add_subdirectory(libraries)
add_subdirectory(misc)
add_subdirectory(utilities)
add_subdirectory(common)

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
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    find_package(LibPThreadsWin32)
    if(LIBPTHREADSWIN32_FOUND)
        set(WIN_RUNTIME_ITEMS ${WIN_RUNTIME_ITEMS}
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/pthreads-win32/COPYING.LIB.txt"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/pthreadVC2.dll")

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/pthreads-win32/COPYING.LIB.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBPTHREADSWIN32_LIB_COPYING}"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/pthreads-win32/COPYING.LIB.txt"
            COMMENT "Copying pthreads-win32 license to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/pthreadVC2.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBPTHREADSWIN32_DLL}"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/"
            COMMENT "Copying pthreadVC2.dll to output directory..."
        )
    endif(LIBPTHREADSWIN32_FOUND)

    if(LIBUSB_FOUND)
        set(WIN_RUNTIME_ITEMS ${WIN_RUNTIME_ITEMS}
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/libusb/COPYING"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/libusb-1.0.dll")

        # The libusb Windows release does not appear to include a COPYING file.
        # Since it's LGPLv2.1, we'll copy the equivalent license from our
        # license directory
        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/libusb/COPYING"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.LGPLv2.1"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/libusb/COPYING"
            COMMENT "Copying libusb license to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/libusb-1.0.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBUSB_PATH}/${LIBUSB_LIBRARY_PATH_SUFFIX}/libusb-1.0.dll"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/"
            COMMENT "Copying libusb-1.0.dll to output directory..."
        )
    endif(LIBUSB_FOUND)

    if(CYAPI_FOUND)
        set(WIN_RUNTIME_ITEMS ${WIN_RUNTIME_ITEMS}
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/Cypress/license.txt"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.inf"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.sys"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.cat"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/${CYUSB3_WDF_COINSTALLER}"
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/Cypress/license.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CYPRESS_LICENSE}"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/Cypress/license.txt"
            COMMENT "Copying Cypress License to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.inf"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${CYUSB3_INF}"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/"
            COMMENT "Copying cyusb3.inf to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.sys"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CYUSB3_DRIVER_DIR}/cyusb3.sys"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/"
            COMMENT "Copying cyusb3.sys to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/cyusb3.cat"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CYUSB3_DRIVER_DIR}/cyusb3.cat"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/"
            COMMENT "Copying cyusb3.cat to output directory..."
        )

        add_custom_command(
            OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/${CYUSB3_WDF_COINSTALLER}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CYUSB3_DRIVER_DIR}/${CYUSB3_WDF_COINSTALLER}"
                "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/drivers/CyUSB3/"
            COMMENT "Copying ${CYUSB3_WDF_COINSTALLER} to output directory..."
        )
    endif()

    set(WIN_RUNTIME_ITEMS ${WIN_RUNTIME_ITEMS}
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/COPYING"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.nuand"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.msc_stdbool"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.nuand"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.LGPLv2.1"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.GPLv2"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.vlfeat"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.msc_inttypes"
    )

    add_custom_command(
        OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/COPYING"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../COPYING"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/COPYING"
        COMMENT "Copying bladeRF COPYING file to output directory..."
    )

    add_custom_command(
        OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.nuand"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.MIT.nuand"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.nuand"
        COMMENT "Copying bladeRF LICENSE.MIT.nuand files to output directory..."
    )

    add_custom_command(
        OUTPUT
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.msc_stdbool"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.MIT.msc_stdbool"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.MIT.msc_stdbool"
        COMMENT "Copying bladeRF LICENSE.MIT.msc_stdbool file to output directory..."
    )

    add_custom_command(
        OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.nuand"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.BSD.nuand"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.nuand"
        COMMENT "Copying bladeRF LICENSE.BSD.nuand file to output directory..."
    )

    add_custom_command(
        OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.LGPLv2.1"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.LGPLv2.1"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.LGPLv2.1"
        COMMENT "Copying bladeRF LICENSE.LGPLv2.1 file to output directory..."
    )

    add_custom_command(
        OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.GPLv2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.GPLv2"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.GPLv2"
        COMMENT "Copying bladeRF LICENSE.GPLv2 file to output directory..."
    )

    add_custom_command(
        OUTPUT
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.vlfeat"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.BSD.vlfeat"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.vlfeat"
        COMMENT "Copying bladeRF LICENSE.BSD.vlfeat file to output directory..."
    )

    add_custom_command(
        OUTPUT
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.msc_inttypes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../legal/licenses/LICENSE.BSD.msc_inttypes"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/licenses/bladeRF/LICENSE.BSD.msc_inttypes"
        COMMENT "Copying bladeRF LICENSE.BSD.msc_inttypes file to output directory..."
    )

    add_custom_target(win_runtime ALL DEPENDS ${WIN_RUNTIME_ITEMS})

endif(WIN32 AND NOT CMAKE_CROSSCOMPILING)
