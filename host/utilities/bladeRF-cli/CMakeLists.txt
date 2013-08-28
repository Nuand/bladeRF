# Copyright 2013 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
cmake_minimum_required(VERSION 2.8)
project(bladeRF-cli C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  3)
set(VERSION_INFO_PATCH  1)
set(VERSION_INFO_EXTRA "git")
include(Version)

set(VERSION "${VERSION_INFO}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY
)

################################################################################
# Configuration options
################################################################################

option(ENABLE_INTERACTIVE_TECLA
       "Enable interactive mode support via libtecla"
       ON
)

################################################################################
# Include paths
################################################################################
set(CLI_INCLUDE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}/src
       ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd
       ${CMAKE_CURRENT_SOURCE_DIR}/src/interactive
       ${CMAKE_CURRENT_BINARY_DIR}/src
       ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
       ${libbladeRF_SOURCE_DIR}/include
)

if(ENABLE_INTERACTIVE_TECLA)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${LIBTECLA_INCLUDE_DIR})
endif()

include_directories(${CLI_INCLUDE_DIRS})

################################################################################
# Configure source files
################################################################################
set(BLADERF_CLI_SOURCE
        src/main.c
        src/common.c
        src/cmd/cmd.c
        src/cmd/load.c
        src/cmd/open.c
        src/cmd/peek.c
        src/cmd/peekpoke.c
        src/cmd/poke.c
        src/cmd/printset.c
        src/cmd/probe.c
        src/cmd/rxtx.c
        src/cmd/version.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(ENABLE_INTERACTIVE_TECLA)
    add_definitions(-DINTERACTIVE=y)
    set (BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/interactive/interactive.c)
endif()

add_executable(bladeRF-cli ${BLADERF_CLI_SOURCE})

################################################################################
# Build dependencies
################################################################################
find_package(PkgConfig)
find_package(Threads)

set(CLI_LINK_LIBRARIES
    libbladerf_shared
    ${CMAKE_THREAD_LIBS_INIT}
)	

if(ENABLE_INTERACTIVE_TECLA)
    find_package(LibTecla)
    if(NOT LIBTECLA_FOUND)
        message(FATAL_ERROR
                "ENABLE_INTERACTIVE_TECLA set, but libtecla could not be found")
    endif(NOT LIBTECLA_FOUND)

    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBTECLA_LIBRARIES} ncurses)

endif(ENABLE_INTERACTIVE_TECLA)

################################################################################
# Build configuration
################################################################################
target_link_libraries(bladeRF-cli ${CLI_LINK_LIBRARIES})

################################################################################
# Installation
################################################################################
if(NOT DEFINED BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR bin)
endif()

install(TARGETS bladeRF-cli DESTINATION ${BIN_INSTALL_DIR})

################################################################################
# Informational output
################################################################################
message(STATUS "Configured to build bladeRF-cli version: ${VERSION_INFO}")
