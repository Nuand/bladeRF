################################################################################
# Build bladeRF FX3 Firmware
################################################################################
cmake_minimum_required(VERSION 2.8.3)
project(bladeRF_fw C)

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
    message(STATUS
        "Build type not specified. Defaulting to: ${CMAKE_BUILD_TYPE}"
    )
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Build type")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../host/cmake/modules)

################################################################################
# Version configuration
################################################################################

# Update these definitions when updating the firmware version
set(VERSION_INFO_MAJOR 2)
set(VERSION_INFO_MINOR 3)
set(VERSION_INFO_PATCH 2)

if(NOT DEFINED VERSION_INFO_EXTRA)
    set(VERSION_INFO_EXTRA "git")
endif()
include(Version)

set(VERSION "${VERSION_INFO}")

# Generate firmware version header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)

################################################################################
# Configuration options
################################################################################

option(ENABLE_BLADERF_FIRMWARE
    "Build firmware for the bladeRF 1.0"
    ON
)

################################################################################
# Build Configuration
################################################################################

# Paths for input files
set(INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../firmware_common"
    "${FX3_INCLUDE_DIR}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Base filenames for output files
set(BLADERF_FW bladeRF_fw_v${VERSION_INFO_BASE})

# Common source files
set(SRC_BLADERF
    "${SRC_DIR}/bladeRF.c"
    "${SRC_DIR}/cyfxbladeRFusbdscr.c"
    "${SRC_DIR}/flash.c"
    "${SRC_DIR}/fpga.c"
    "${SRC_DIR}/gpif.c"
    "${SRC_DIR}/logger.c"
    "${SRC_DIR}/rf.c"
    "${SRC_DIR}/spi_flash_lib.c"
    "${FX3_FW_COMMON_DIR}/cyfx_gcc_startup.S"
    "${FX3_FW_COMMON_DIR}/cyfxtx.c"
    "${SRC_DIR}/bladeRF1.c"
    "${SRC_DIR}/bladeRF2.c"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-O0 -g")
else()
    set(CMAKE_C_FLAGS "-O2")
endif()

set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -DCYU3P_FX3=1 \
     -D__CYU3P_TX__=1 -mcpu=arm926ej-s -mthumb-interwork"
)

include_directories(${INCLUDE_DIRS})
enable_language(ASM)

set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,--entry,CyU3PFirmwareEntry \
     -Wl,-T,\"${FX3_LINKER_FILE}\" \
     -Wl,-d -Wl,--gc-sections \
     -Wl,--no-wchar-size-warning"
)

################################################################################
# Build or find required host utililities
################################################################################

if(FX3_WINDOWS_HOST)
    set(ELF2IMG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/elf2img.exe)

    # A prebuild-elf2img binary ships with the FX3 SDK.
    # Just copy it to our output directory for simplicity.
    add_custom_command(
        OUTPUT ${ELF2IMG}
        COMMAND ${CMAKE_COMMAND} -E copy ${FX3_ELF2IMG} ${ELF2IMG}
    )

    add_custom_target(elf2img ALL DEPENDS ${ELF2IMG})
else()
    # Otherwise, we need to build it ourselves
    include(ExternalProject)
    ExternalProject_Add(elf2img
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/util/elf2img/"
        CMAKE_ARGS "-DFX3_ELF2IMG=${FX3_ELF2IMG};-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        INSTALL_COMMAND ""
    )
    set(ELF2IMG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/elf2img)
endif()

################################################################################
# Build firmware ELF
################################################################################

# Build an archive of the common object files.
add_library(bladeRF_fx3 STATIC ${SRC_BLADERF})
target_link_libraries(bladeRF_fx3
    "${FX3_LIBRARY_DIR}/libcyfxapi.a"
    "${FX3_LIBRARY_DIR}/libcyu3lpp.a"
    "${FX3_LIBRARY_DIR}/libcyu3threadx.a"
)

if(ENABLE_BLADERF_FIRMWARE)
    add_executable(${BLADERF_FW}.elf ${SRC_BLADERF})
    target_link_libraries(${BLADERF_FW}.elf
        "${FX3_LIBRARY_DIR}/libcyfxapi.a"
        "${FX3_LIBRARY_DIR}/libcyu3lpp.a"
        "${FX3_LIBRARY_DIR}/libcyu3threadx.a"
        "${ARM_NONE_EABI_LIBC}"
        "${ARM_NONE_EABI_LIBGCC}"
        bladeRF_fx3
    )
    set_target_properties(${BLADERF_FW}.elf
        PROPERTIES LINK_FLAGS "-Wl,-Map,output/${BLADERF_FW}.map")
endif()

################################################################################
# Create bootable image from firmware ELF
################################################################################

set(ELF2IMG_WORK_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(ENABLE_BLADERF_FIRMWARE)
    add_custom_command(
        OUTPUT ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.img
        COMMAND ${ELF2IMG} -i ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.elf -o ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.img
        DEPENDS ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.elf
        COMMENT "Creating ${BLADERF_FW}.img. Interrupt vector removal note is expected."
    )

    add_custom_target(${BLADERF_FW}.img ALL
        DEPENDS elf2img ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.elf ${ELF2IMG_WORK_DIR}/${BLADERF_FW}.img)
endif()

################################################################################
# Drop our .gdbinit sample into our build output directory for convenience
################################################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdb/gdbinit_sample
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.gdbinit
               @ONLY)
