cmake_minimum_required(VERSION 3.5)
project(ad936x C)

message(STATUS "libad936x: Configuring build...")

option(NIOS_HAS_HARDWARE_MULTIPLY_DIVIDE
       "Use hardware multiply/divide in Nios II/f"
       OFF
)

option(BUILD_AD936X_PRINTF
       "Enable console printf output for the AD936x library."
       OFF
)

option(BUILD_AD936X_PRINTF_DEBUG
       "Enable debugging output for the AD936x library."
       OFF
)

################################################################################
# Paths
################################################################################

# Repository root
set(LIBAD936X_BLADERF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
# Source directory for local files (patches, platform_bladerf2, etc)
set(LIBAD936X_LOCAL_DIR "${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS_local")
# libbladeRF relative path
set(LIBAD936X_LIBBLADERF_DIR "${LIBAD936X_BLADERF_ROOT}/host/libraries/libbladeRF")
# fpga_common relative path
set(LIBAD936X_FPGACOMMON_DIR "${LIBAD936X_BLADERF_ROOT}/fpga_common")

# Patch files live here
set(LIBAD936X_PATCH_DIR "${LIBAD936X_LOCAL_DIR}/patches")
# Platform files
set(LIBAD936X_PLATFORM_DIR "${LIBAD936X_LOCAL_DIR}/platform_bladerf2_headless")

# Upstream source (clean)
set(LIBAD936X_UPSTREAM_DIR "${LIBAD936X_LOCAL_DIR}/../no-OS/ad9361/sw")
# Upstream source (patched)
set(LIBAD936X_UPSTREAM_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Use CMake modules from the host tree
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LIBAD936X_BLADERF_ROOT}/host/cmake/modules/")


# Ensure the toolchain file got loaded...
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "nios2")
    message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}, expected nios2")
endif()


################################################################################
# Configure build
################################################################################

add_library(ad936x STATIC
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_api.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_conv.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/util.c
    ${LIBAD936X_PLATFORM_DIR}/platform.c
    ${LIBAD936X_PLATFORM_DIR}/adc_core.c
    ${LIBAD936X_PLATFORM_DIR}/dac_core.c
    ${LIBAD936X_FPGACOMMON_DIR}/src/ad936x_params.c
)

target_compile_options(ad936x PRIVATE
    -DBLADERF_NIOS_BUILD -DNUAND_MODIFICATIONS -Wall -Os
)

if(NOT NIOS_HAS_HARDWARE_MULTIPLY_DIVIDE)
    message(STATUS "libad936x: building with software mul/mulx/div operations")
    target_compile_options(ad936x PRIVATE 
        -mno-hw-div -mno-hw-mul -mno-hw-mulx
    )
endif(NOT NIOS_HAS_HARDWARE_MULTIPLY_DIVIDE)

if(NOT MSVC)
    target_compile_options(ad936x PRIVATE
        -ffunction-sections -fdata-sections
        -Wno-unused-but-set-variable -std=gnu11
    )
endif(NOT MSVC)

# Main Nios app build uses -fshort-enums, so we need to use it here too
target_compile_options(ad936x PRIVATE
    -fshort-enums
)

if(BUILD_AD936X_PRINTF OR BUILD_AD936X_PRINTF_DEBUG)
    message(STATUS "libad936x: Enabling console output")
    target_compile_options(ad936x PRIVATE -DHAVE_VERBOSE_MESSAGES)
endif(BUILD_AD936X_PRINTF OR BUILD_AD936X_PRINTF_DEBUG)

if(BUILD_AD936X_PRINTF_DEBUG)
    message(STATUS "libad936x: Enabling full debugging output")
    target_compile_options(ad936x PRIVATE -DHAVE_DEBUG_MESSAGES)
endif(BUILD_AD936X_PRINTF_DEBUG)

target_include_directories(ad936x PUBLIC
    ${CMAKE_SOURCE_DIR}/include

    ${LIBAD936X_PLATFORM_DIR}
    ${LIBAD936X_UPSTREAM_BUILD_DIR}
    ${LIBAD936X_LIBBLADERF_DIR}/src
    ${LIBAD936X_LIBBLADERF_DIR}/include

    ${LIBAD936X_BLADERF_ROOT}/host/common/include
    ${LIBAD936X_BLADERF_ROOT}/firmware_common

    ${LIBAD936X_BLADERF_ROOT}/hdl/fpga/platforms/common/bladerf/software/bladeRF_nios/src
    ${LIBAD936X_BLADERF_ROOT}/hdl/fpga/platforms/bladerf-micro/software/bladeRF_nios/src
    ${LIBAD936X_BLADERF_ROOT}/fpga_common/include
    ${CMAKE_BINARY_DIR}/../bladeRF_nios_bsp
    ${CMAKE_BINARY_DIR}/../bladeRF_nios_bsp/drivers/inc
    ${CMAKE_BINARY_DIR}/../bladeRF_nios_bsp/HAL/inc
)

################################################################################
# Dependencies
################################################################################

# If the submodule hasn't been initialized/updated, do so
if(NOT IS_DIRECTORY ${LIBAD936X_UPSTREAM_DIR})
    message(STATUS "libad936x: Attempting to initialize missing submodule")

    find_package(Git QUIET)

    if(Git_FOUND)
        execute_process(
            COMMAND ${Git_EXECUTABLE} submodule init
            WORKING_DIRECTORY ${LIBAD936X_BLADERF_ROOT}
        )
        execute_process(
            COMMAND ${Git_EXECUTABLE} submodule update
            WORKING_DIRECTORY ${LIBAD936X_BLADERF_ROOT}
        )
    endif()
endif()

# Then, re-check for submodule...
if(NOT IS_DIRECTORY ${LIBAD936X_UPSTREAM_DIR})
    message(FATAL_ERROR "Directory ${LIBAD936X_UPSTREAM_DIR} was not found. "
                        "It appears that this submodule has not been properly "
                        "initialized. Please run 'git submodule init' and "
                        "'git submodule update', and then re-run cmake.")
endif()


################################################################################
# Copy files and apply patches
################################################################################

# Copy needed files from the upstream repository
set(LIBAD936X_UPSTREAM_FILES
    ad9361.c
    ad9361.h
    ad9361_api.c
    ad9361_api.h
    ad9361_conv.c
    common.h
    util.c
    util.h
)

foreach(cpfile IN ITEMS ${LIBAD936X_UPSTREAM_FILES})
    configure_file(
        "${LIBAD936X_UPSTREAM_DIR}/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# Patch the upstream files
set(LIBAD936X_PATCHES
    0001-0bba46e-nuand-modifications.patch
    0002-0bba46e-pr-561.patch
    0003-0bba46e-pr-573.patch
    0004-0bba46e-pr-598.patch
    0005-0bba46e-rm-ad9361_parse_fir.patch
    0006-0bba46e-compilefix.patch
)

find_package(Patcher REQUIRED)

foreach(patchfile IN ITEMS ${LIBAD936X_PATCHES})
    message(STATUS "libad936x: Applying patch: ${patchfile}")
    execute_process(
        COMMAND ${Patcher_EXECUTABLE} -p3
        INPUT_FILE ${LIBAD936X_PATCH_DIR}/${patchfile}
        WORKING_DIRECTORY "${LIBAD936X_UPSTREAM_BUILD_DIR}"
        OUTPUT_QUIET
        RESULT_VARIABLE resultcode
        ERROR_VARIABLE errvar
    )

    if(resultcode)
        message(FATAL_ERROR "Failed to apply ${patchfile}: ${errvar}")
    endif(resultcode)
endforeach(patchfile)

message(STATUS "libad936x: Build configured")
