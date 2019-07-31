cmake_minimum_required(VERSION 2.8)
project(bladeRF-cli C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  1)
set(VERSION_INFO_MINOR  8)
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
# Build dependencies
################################################################################
find_package(LibTecla)
find_package(LibEdit)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
else(MSVC)
	find_package(Threads REQUIRED)
endif(MSVC)

find_program(PANDOC_EXECUTABLE pandoc)
find_program(BASH_EXECUTABLE bash)

if(PANDOC_EXECUTABLE AND BASH_EXECUTABLE)
    set(CAN_BUILD_INTERACTIVE_HELP ON)
else()
    set(CAN_BUILD_INTERACTIVE_HELP OFF)
endif()

################################################################################
# Configuration options
################################################################################

option(ENABLE_LIBTECLA
        "Enable the use of libtecla, if available.")

option(ENABLE_LIBEDIT
        "Enable the use of libtecla, if available.")

option(BUILD_BLADERF_CLI_DOCUMENTATION
        "Build bladeRF-cli man page. Requires help2man."
        ${BUILD_DOCUMENTATION})

option(BUILD_BLADERF_CLI_INTERACTIVE_HELP
        "Rebuild bladeRF-cli interactive help. Requires pandoc."
        ${CAN_BUILD_INTERACTIVE_HELP})

if(NOT ${BUILD_DOCUMENTATION})
    set(BUILD_BLADERF_CLI_DOCUMENTATION OFF)
endif()

if((NOT CAN_BUILD_INTERACTIVE_HELP) AND (BUILD_BLADERF_CLI_INTERACTIVE_HELP))
    message(FATAL_ERROR "BUILD_BLADERF_CLI_INTERACTIVE_HELP was forced ON, "
            "but there are missing prerequisites!")
endif()

if(ENABLE_LIBTECLA)
    if((NOT LIBTECLA_INCLUDE_DIR) OR (NOT LIBTECLA_LIBRARIES))
        message(FATAL_ERROR "ENABLE_LIBTECLA was forced ON, but libtecla "
                "cannot be found!")
    endif()
endif()
if(ENABLE_LIBEDIT)
    if((NOT LIBEDIT_INCLUDE_DIR) OR (NOT LIBEDIT_LIBRARIES))
        message(FATAL_ERROR "ENABLE_LIBEDIT was forced ON, but editline "
                "cannot be found!")
    endif()
endif()

################################################################################
# Regenerate interactive help
################################################################################
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc)

if(BUILD_BLADERF_CLI_INTERACTIVE_HELP)
    # Do a quick test to make sure we have everything we need...
    execute_process(
        COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/generate.bash
                --pandoc=${PANDOC_EXECUTABLE}
                --test
                ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/interactive-help.md
        RESULT_VARIABLE INTERACTIVE_HELP_GENERATOR_FAILED
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc
    )

    if(INTERACTIVE_HELP_GENERATOR_FAILED)
        message(FATAL_ERROR "generate.bash failed! Try disabling "
                "BUILD_BLADERF_CLI_INTERACTIVE_HELP, or fix the above error.")
    endif()

    # Run the generation script during build
    message(STATUS "Will regenerate bladeRF-cli interactive help")
    add_custom_command(
        OUTPUT src/cmd/doc/cmd_help.h src/cmd/doc/cmd_help.man
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/generate.bash
        COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/generate.bash
                --pandoc=${PANDOC_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/interactive-help.md
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc
    )
else()
    # Re-use old cmd_help.h and cmd_help.man
    message(STATUS "Reusing existing interactive help for bladeRF-cli")
    add_custom_command(
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/cmd_help.h.in
        OUTPUT src/cmd/doc/cmd_help.h
        COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/cmd_help.h.in
            ${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc/cmd_help.h
    )
    add_custom_command(
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/cmd_help.man.in
        OUTPUT src/cmd/doc/cmd_help.man
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd/doc/cmd_help.man.in
            ${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc/cmd_help.man
    )
endif()

################################################################################
# Include paths
################################################################################
set(CLI_INCLUDE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}/src
       ${CMAKE_CURRENT_SOURCE_DIR}/src/cmd
       ${CMAKE_CURRENT_SOURCE_DIR}/src/input
       ${CMAKE_CURRENT_BINARY_DIR}/src
       ${CMAKE_CURRENT_BINARY_DIR}/src/cmd
       ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
       ${libbladeRF_SOURCE_DIR}/include
       ${BLADERF_FW_COMMON_INCLUDE_DIR}
)

if(MSVC)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${MSVC_C99_INCLUDES})
endif()


message(STATUS "libtecla ${ENABLE_LIBTECLA} ${LIBTECLA_FOUND}")
message(STATUS "libedit ${ENABLE_LIBEDIT} ${LIBEDIT_FOUND}")
if(ENABLE_LIBTECLA OR (LIBTECLA_FOUND AND NOT ENABLE_LIBEDIT))
    message(STATUS "libtecla support enabled")
    set(ENABLE_LIBTECLA ON)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${LIBTECLA_INCLUDE_DIR})
elseif(ENABLE_LIBEDIT OR LIBEDIT_FOUND)
    message(STATUS "editline support enabled")
    set(ENABLE_LIBEDIT ON)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS} ${LIBEDIT_INCLUDE_DIR})
endif()

if(MSVC)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/windows
        ${LIBPTHREADSWIN32_INCLUDE_DIRS}
    )
endif()

if(APPLE)
    set(CLI_INCLUDE_DIRS ${CLI_INCLUDE_DIRS}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/osx
    )
endif()

include_directories(${CLI_INCLUDE_DIRS})

################################################################################
# Configure source files
################################################################################
set(BLADERF_CLI_SOURCE
        src/main.c
        src/common.c
        src/cmd/calibrate.c
        src/cmd/cmd.c
        src/cmd/doc/cmd_help.h
        src/cmd/erase.c
        src/cmd/flash_backup.c
        src/cmd/flash_image.c
        src/cmd/flash_init_cal.c
        src/cmd/flash_restore.c
        src/cmd/fw_log.c
        src/cmd/info.c
        src/cmd/jump_boot.c
        src/cmd/lms_reg_info.c
        src/cmd/load.c
        src/cmd/mimo.c
        src/cmd/open.c
        src/cmd/peek.c
        src/cmd/peekpoke.c
        src/cmd/poke.c
        src/cmd/printset.c
        src/cmd/printset_hardware.c
        src/cmd/printset_impl.c
        src/cmd/printset_xb.c
        src/cmd/probe.c
        src/cmd/recover.c
        src/cmd/rx.c
        src/cmd/rxtx.c
        src/cmd/trigger.c
        src/cmd/tx.c
        src/cmd/version.c
        src/cmd/xb.c
        src/cmd/xb100.c
        src/cmd/xb200.c
        src/cmd/xb300.c
        src/input/input.c
        src/input/script.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/dc_calibration.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/str_queue.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/parse.c
)

# Select the input mode (and script handling) backend
if(ENABLE_LIBTECLA)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/input/tecla.c)
elseif(ENABLE_LIBEDIT)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/input/editline.c)
else()
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE} src/input/fgets.c)
endif()

if(MSVC)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/nanosleep.c
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/setenv.c
    )
endif()

if(APPLE)
    set(BLADERF_CLI_SOURCE ${BLADERF_CLI_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
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

if(NOT MSVC)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} m)
endif()

if(ENABLE_LIBTECLA)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBTECLA_LIBRARIES})

    # On a few tests Linux distros, libtecla is not build against
    # ncurses. However, folks have reported that builds on OSX use it
    if(CMAKE_HOST_APPLE)
        set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ncurses)
    endif(CMAKE_HOST_APPLE)
elseif(ENABLE_LIBEDIT)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBEDIT_LIBRARIES})
endif(ENABLE_LIBTECLA)

if(LIBPTHREADSWIN32_FOUND)
    set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

if(LIBC_VERSION)
    # clock_gettime() was moved from librt -> libc in 2.17
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} rt)
    endif()
endif()

target_link_libraries(bladeRF-cli ${CLI_LINK_LIBRARIES})

################################################################################
# Man pages
################################################################################
if(BUILD_BLADERF_CLI_DOCUMENTATION)
    find_program(HELP2MAN_EXECUTABLE help2man)

    if(HELP2MAN_EXECUTABLE)
        message(STATUS "Will build man page for bladeRF-cli.")

        if(NOT DEFINED MAN_INSTALL_DIR)
            set(MAN_INSTALL_DIR share/man)
        endif(NOT DEFINED MAN_INSTALL_DIR)

        # Build actual man page
        add_custom_command(
            OUTPUT bladeRF-cli.1
            DEPENDS bladeRF-cli src/cmd/doc/cmd_help.man
            COMMAND ${HELP2MAN_EXECUTABLE}
                --no-info --no-discard-stderr
                --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/bladeRF-cli.h2m
                --opt-include=${CMAKE_CURRENT_BINARY_DIR}/src/cmd/doc/cmd_help.man
                --opt-include=${CMAKE_CURRENT_SOURCE_DIR}/../doc/include_man/common.h2m
                --output=bladeRF-cli.1
                ./bladeRF-cli
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )

        install(
            FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bladeRF-cli.1
            DESTINATION ${MAN_INSTALL_DIR}/man1
        )

        add_custom_target(bladeRF-cli-manpage ALL DEPENDS bladeRF-cli.1)

    else(HELP2MAN_EXECUTABLE)
        message(FATAL_ERROR "Could not find help2man. bladeRF-cli man page "
                "cannot be built.")
    endif(HELP2MAN_EXECUTABLE)
else(BUILD_BLADERF_CLI_DOCUMENTATION)
    message(STATUS "Not building man page for bladeRF-cli. "
            "(BUILD_BLADERF_CLI_DOCUMENTATION is OFF)")
endif(BUILD_BLADERF_CLI_DOCUMENTATION)

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
