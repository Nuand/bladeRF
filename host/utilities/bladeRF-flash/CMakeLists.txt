cmake_minimum_required(VERSION 2.8)
project(bladeRF-flash C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  4)
set(VERSION_INFO_PATCH  2)
if(NOT DEFINED VERSION_INFO_EXTRA)
    set(VERSION_INFO_EXTRA "git")
endif()
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
# Configuration options
################################################################################

option(BUILD_BLADERF_FLASH_DOCUMENTATION
        "Build bladeRF-flash man page. Requires help2man."
        ${BUILD_DOCUMENTATION})

if(NOT ${BUILD_DOCUMENTATION})
    set(BUILD_BLADERF_FLASH_DOCUMENTATION OFF)
endif()


################################################################################
# Include paths
################################################################################
set(FLASH_INCLUDE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}/src
       ${CMAKE_CURRENT_BINARY_DIR}/src
       ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
       ${libbladeRF_SOURCE_DIR}/include
       ${BLADERF_FW_COMMON_INCLUDE_DIR}
       ${UTILITIES_COMMON_INC_DIR}
       ${LIBUSB_INCLUDE_DIRS})

include_directories(${FLASH_INCLUDE_DIRS})

################################################################################
# Configure source files
################################################################################
set(BLADERF_FLASH_SOURCE
        src/main.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
        ${UTILITIES_COMMON_SRC}/ezusb.c
)

if(MSVC)
    set(BLADERF_FLASH_SOURCE ${BLADERF_FLASH_SOURCE}
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
# Man pages
################################################################################
if(BUILD_BLADERF_FLASH_DOCUMENTATION)
    find_program(HELP2MAN_EXECUTABLE help2man)

    if(HELP2MAN_EXECUTABLE)
        message(STATUS "Will build man page for bladeRF-flash.")

        if(NOT DEFINED MAN_INSTALL_DIR)
            set(MAN_INSTALL_DIR share/man)
        endif(NOT DEFINED MAN_INSTALL_DIR)

        # Build interactive commands summary, for inclusion in man page.
        # bladeRF-flash doesn't have an interactive mode
        #add_custom_command(
        #    OUTPUT bladeRF-flash_auto.h2m
        #    DEPENDS bladeRF-flash
        #    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/interrogate.py
        #                   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bladeRF-flash > bladeRF-flash_auto.h2m
        #    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        #)

        # Build actual man page
        add_custom_command(
            OUTPUT bladeRF-flash.1
            DEPENDS bladeRF-flash #bladeRF-flash_auto.h2m
            COMMAND ${HELP2MAN_EXECUTABLE} --no-info
                                           --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/bladeRF-flash.h2m
                                           #--opt-include=${CMAKE_CURRENT_BINARY_DIR}/bladeRF-flash_auto.h2m
                                           --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/common.h2m
                                           --output=bladeRF-flash.1 ./bladeRF-flash
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )

        install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bladeRF-flash.1 DESTINATION ${MAN_INSTALL_DIR}/man1)

        add_custom_target(bladeRF-flash-manpage ALL DEPENDS bladeRF-flash.1)

    else(HELP2MAN_EXECUTABLE)
        message(WARNING "Could not find help2man. bladeRF-flash man page will not be built.")
    endif(HELP2MAN_EXECUTABLE)
else(BUILD_BLADERF_FLASH_DOCUMENTATION)
    message(STATUS "Not building man page for bladeRF-flash.  (BUILD_BLADERF_FLASH_DOCUMENTATION is OFF)")
endif(BUILD_BLADERF_FLASH_DOCUMENTATION)

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
