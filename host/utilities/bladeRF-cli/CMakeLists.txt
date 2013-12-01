cmake_minimum_required(VERSION 2.8)
project(bladeRF-cli C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  9)
set(VERSION_INFO_PATCH  0)
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
# Configuration options
################################################################################

find_package(LibTecla)

option(ENABLE_LIBTECLA
        "Enable the use of libtecla, if available."
        ${LIBTECLA_FOUND}
)

if(ENABLE_LIBTECLA)
    message(STATUS "libtecla support enabled")
endif()

find_package(LibUSB)
if (LIBUSB_FOUND)
    add_definitions(-DCLI_LIBUSB_ENABLED)
endif()



################################################################################
# Build dependencies
################################################################################

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
else(MSVC)
	find_package(Threads REQUIRED)
endif(MSVC)


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
       ${BLADERF_FW_COMMON_INCLUDE_DIR}
       ${UTILITIES_COMMON_INC_DIR}
)


if(ENABLE_LIBTECLA)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${LIBTECLA_INCLUDE_DIR})
endif()

if(LIBUSB_FOUND)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
endif()

if(MSVC)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/windows
        ${UTILITIES_COMMON_INC_DIR}/windows
        ${LIBPTHREADSWIN32_INCLUDE_DIRS}
    )
endif()

if(APPLE)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS}
        ${UTILITIES_COMMON_INC_DIR}/osx
    )
endif()

include_directories(${CLI_INCLUDE_DIRS})

################################################################################
# Configure source files
################################################################################
set(BLADERF_CLI_SOURCE
        src/main.c
        src/common.c
        src/script.c
        src/cmd/calibrate.c
        src/cmd/cmd.c
        src/cmd/erase.c
        src/cmd/flash_backup.c
        src/cmd/flash_image.c
        src/cmd/flash_init_cal.c
        src/cmd/flash_restore.c
        src/cmd/info.c
        src/cmd/load.c
        src/cmd/open.c
        src/cmd/peek.c
        src/cmd/peekpoke.c
        src/cmd/poke.c
        src/cmd/printset.c
        src/cmd/probe.c
        src/cmd/rx.c
        src/cmd/rxtx.c
        src/cmd/tx.c
        src/cmd/version.c
        src/cmd/recover.c
        src/cmd/jump_boot.c
        src/cmd/mimo.c
        src/cmd/dc_gain.c
        src/interactive/interactive.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
        ${UTILITIES_COMMON_SRC}/ezusb.c
)

# Select the interactive mode (and script handling) backend
if(ENABLE_LIBTECLA)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/interactive/tecla.c)
else()
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/interactive/fgets.c)
endif()

if(MSVC)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
            ${UTILITIES_COMMON_SRC}/windows/clock_gettime.c
    )
endif()

if(APPLE)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE}
            ${UTILITIES_COMMON_SRC}/osx/clock_gettime.c
    )
endif()

add_executable(bladeRF-cli ${BLADERF_CLI_SOURCE})

################################################################################
# Build configuration
################################################################################
set(CLI_LINK_LIBRARIES
    libbladerf_shared
    ${CMAKE_THREAD_LIBS_INIT}
)

if(ENABLE_LIBTECLA)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBTECLA_LIBRARIES})

    # On a few tests Linux distros, libtecla is not build against
    # ncurses. However, folks have reported that builds on OSX use it
    if(CMAKE_HOST_APPLE)
        set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ncurses)
    endif(CMAKE_HOST_APPLE)

endif(ENABLE_LIBTECLA)

if(LIBUSB_FOUND)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBUSB_LIBRARIES})
endif()

if(LIBPTHREADSWIN32_FOUND)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

target_link_libraries(bladeRF-cli ${CLI_LINK_LIBRARIES})

################################################################################
# Man pages
################################################################################
if(BUILD_DOCUMENTATION)
    find_program(HELP2MAN_EXECUTABLE help2man)

    if(HELP2MAN_EXECUTABLE)
        message(STATUS "Will build man page for bladeRF-cli.")

        if(NOT DEFINED MAN_INSTALL_DIR)
            set(MAN_INSTALL_DIR share/man)
        endif(NOT DEFINED MAN_INSTALL_DIR)

        # Build interactive commands summary, for inclusion in man page
        add_custom_command(
            OUTPUT bladeRF-cli_auto.h2m
            DEPENDS bladeRF-cli
            COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/interrogate.py
                           ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bladeRF-cli > bladeRF-cli_auto.h2m
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )

        # Build actual man page
        add_custom_command(
            OUTPUT bladeRF-cli.1
            DEPENDS bladeRF-cli bladeRF-cli_auto.h2m
            COMMAND ${HELP2MAN_EXECUTABLE} --no-info
                                           --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/bladeRF-cli.h2m
                                           --opt-include=${CMAKE_CURRENT_BINARY_DIR}/bladeRF-cli_auto.h2m
                                           --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/common.h2m
                                           --output=bladeRF-cli.1 ./bladeRF-cli
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )

        install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bladeRF-cli.1 DESTINATION ${MAN_INSTALL_DIR}/man1)

        add_custom_target(bladeRF-cli-manpage ALL SOURCES bladeRF-cli.1)

    else(HELP2MAN_EXECUTABLE)
        message(WARNING "Could not find help2man. bladeRF-cli man page will not be built.")
    endif(HELP2MAN_EXECUTABLE)
else(BUILD_DOCUMENTATION)
    message(STATUS "Not building man page for bladeRF-cli.  (BUILD_DOCUMENTATION is OFF)")
endif(BUILD_DOCUMENTATION)

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
