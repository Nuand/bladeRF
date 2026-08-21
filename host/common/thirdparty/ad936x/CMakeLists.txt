message(STATUS "libad936x: Configuring build...")


################################################################################
# Paths
################################################################################

# Repository root
set(LIBAD936X_BLADERF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
# Source directory for local files (patches, platform_bladerf2, etc)
set(LIBAD936X_LOCAL_DIR "${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS_local")
# libbladeRF relative path
set(LIBAD936X_LIBBLADERF_DIR "${LIBAD936X_BLADERF_ROOT}/host/libraries/libbladeRF")
# fpga_common relative path
set(LIBAD936X_FPGACOMMON_DIR "${LIBAD936X_BLADERF_ROOT}/fpga_common")

# Patch files live here
set(LIBAD936X_PATCH_DIR "${LIBAD936X_LOCAL_DIR}/patches")
# Platform files
set(LIBAD936X_PLATFORM_DIR "${LIBAD936X_LOCAL_DIR}/platform_bladerf2")

# Upstream source (clean)
#
# The driver moved out of ad9361/sw/ into drivers/rf-transceiver/ad9361/ and
# util.c was renamed ad9361_util.c. The shared headers and helpers it now pulls
# in live in include/ and util/.
set(LIBAD936X_UPSTREAM_ROOT "${LIBAD936X_LOCAL_DIR}/../no-OS")
set(LIBAD936X_UPSTREAM_DIR "${LIBAD936X_UPSTREAM_ROOT}/drivers/rf-transceiver/ad9361")
set(LIBAD936X_UPSTREAM_INCLUDE_DIR "${LIBAD936X_UPSTREAM_ROOT}/include")
set(LIBAD936X_UPSTREAM_UTIL_DIR "${LIBAD936X_UPSTREAM_ROOT}/util")
set(LIBAD936X_UPSTREAM_API_DIR "${LIBAD936X_UPSTREAM_ROOT}/drivers/api")
set(LIBAD936X_UPSTREAM_AXI_DIR "${LIBAD936X_UPSTREAM_ROOT}/drivers/axi_core")
# Upstream source (patched)
set(LIBAD936X_UPSTREAM_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")


################################################################################
# Configure build
################################################################################

add_library(ad936x STATIC
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_api.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_conv.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_util.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_ext_band_ctrl.c
    # Shared helpers the driver now calls through the no-OS headers.
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_alloc.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_clk.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_util.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_mutex.c
    # Generic API layer: dispatches to the platform ops tables in
    # platform_bladerf2.
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_gpio.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_spi.c
    ${LIBAD936X_PLATFORM_DIR}/platform.c
    ${LIBAD936X_PLATFORM_DIR}/bladerf2_spi.c
    ${LIBAD936X_PLATFORM_DIR}/bladerf2_gpio.c
    # adc_core.c and dac_core.c drove the AXI ADC/DAC cores, which this board
    # does not have; config.h defines AXI_ADC_NOT_PRESENT.
    ${LIBAD936X_FPGACOMMON_DIR}/src/ad936x_params.c
)

target_compile_options(ad936x PRIVATE
    -DNUAND_MODIFICATIONS
    -Wall -Os
)

if(NOT MSVC)
    target_compile_options(ad936x PRIVATE
        -ffunction-sections
        -fdata-sections
        -fPIC
        -std=gnu11
    )
endif(NOT MSVC)

if(NOT MSVC AND NOT APPLE)
    target_compile_options(ad936x PRIVATE
        -Wno-unused-but-set-variable
    )
endif(NOT MSVC AND NOT APPLE)

if(APPLE)
    target_compile_options(ad936x PRIVATE
        -Wno-absolute-value
    )
endif(APPLE)

if(BUILD_AD936X_DEBUG)
    message(STATUS "libad936x: Enabling debugging output")
    target_compile_options(ad936x PRIVATE -D_DEBUG)
endif(BUILD_AD936X_DEBUG)

target_include_directories(ad936x PUBLIC
    ${LIBAD936X_PLATFORM_DIR}
    ${LIBAD936X_UPSTREAM_BUILD_DIR}
    ${LIBAD936X_LIBBLADERF_DIR}/src
    ${LIBAD936X_LIBBLADERF_DIR}/include

    ${LIBAD936X_BLADERF_ROOT}/host/common/include
    ${LIBAD936X_BLADERF_ROOT}/firmware_common

    # host_config.h: two possible paths
    # we can't use find_file to find this, as it hasn't been generated yet
    ${CMAKE_CURRENT_BINARY_DIR}/../../../host/common/include
    ${CMAKE_CURRENT_BINARY_DIR}/../../../common/include
)


################################################################################
# Dependencies
################################################################################

if(MSVC)
    add_definitions(-DHAVE_STRUCT_TIMESPEC)
    target_include_directories(ad936x PUBLIC ${MSVC_C99_INCLUDES})
    set(HAVE_THREADS true)
else(MSVC)
    find_package(Threads REQUIRED)
    if(Threads_FOUND)
        set(HAVE_THREADS true)
    else()
        set(HAVE_THREADS false)
    endif()
    target_link_libraries(ad936x ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_include_directories(ad936x PUBLIC /usr/include/malloc)
endif()

# If the submodule hasn't been initialized/updated, do so
if(NOT IS_DIRECTORY ${LIBAD936X_UPSTREAM_DIR})
    message(STATUS "libad936x: Attempting to initialize missing submodule")

    find_package(Git)

    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule init
            WORKING_DIRECTORY ${LIBAD936X_BLADERF_ROOT}
        )
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update
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
    ad9361_util.c
    ad9361_util.h
    ad9361_ext_band_ctrl.h
    ad9361_ext_band_ctrl.c
)

foreach(cpfile IN ITEMS ${LIBAD936X_UPSTREAM_FILES})
    configure_file(
        "${LIBAD936X_UPSTREAM_DIR}/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# Shared no-OS headers the driver includes, and the few helpers it links
# against. Copied alongside the driver so the build directory stays the single
# include root, as it was when the driver lived in one directory.
file(GLOB LIBAD936X_UPSTREAM_NOOS_HEADERS
     "${LIBAD936X_UPSTREAM_INCLUDE_DIR}/no_os_*.h")

foreach(cpfile IN LISTS LIBAD936X_UPSTREAM_NOOS_HEADERS)
    configure_file("${cpfile}" "${LIBAD936X_UPSTREAM_BUILD_DIR}/" COPYONLY)
endforeach(cpfile)

set(LIBAD936X_UPSTREAM_UTIL_FILES
    no_os_alloc.c
    no_os_clk.c
    no_os_util.c
    no_os_mutex.c
)

foreach(cpfile IN ITEMS no_os_gpio.c no_os_spi.c)
    configure_file(
        "${LIBAD936X_UPSTREAM_API_DIR}/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

foreach(cpfile IN ITEMS ${LIBAD936X_UPSTREAM_UTIL_FILES})
    configure_file(
        "${LIBAD936X_UPSTREAM_UTIL_DIR}/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# The driver includes the AXI core headers unconditionally, but every call into
# them is guarded by AXI_ADC_NOT_PRESENT, which config.h defines for this
# board. Headers only; the cores are not built.
configure_file(
    "${LIBAD936X_UPSTREAM_AXI_DIR}/axi_adc_core/axi_adc_core.h"
    "${LIBAD936X_UPSTREAM_BUILD_DIR}/" COPYONLY)
configure_file(
    "${LIBAD936X_UPSTREAM_AXI_DIR}/axi_dac_core/axi_dac_core.h"
    "${LIBAD936X_UPSTREAM_BUILD_DIR}/" COPYONLY)

# No patches are applied any more.
#
# The six patches this tree used to carry were written against 0bba46e (2018)
# and every one of them is now unnecessary:
#
#   0001 nuand-modifications  substituted the SPI accessors inside the driver
#                             body behind #ifdef NUAND_MODIFICATIONS. The
#                             driver now takes the transfer through
#                             struct no_os_spi_platform_ops, so the
#                             substitution lives in platform_bladerf2 instead
#                             (bladerf2_spi.c) and the driver source is used
#                             unmodified.
#   0002 pr-561               FreeBSD build guard; superseded upstream.
#   0003 pr-573               uintptr_t cast in a return; the affected code is
#                             gone.
#   0004 pr-598               lowered MIN_CARRIER_FREQ_HZ to 47 MHz. Upstream
#                             now splits the limit per direction:
#                             MIN_RX_CARRIER_FREQ_HZ 70 MHz and
#                             MIN_TX_CARRIER_FREQ_HZ 46875001 Hz, which is what
#                             this board reports for RX and TX respectively.
#   0005 rm-ad9361_parse_fir  removed a parser that is no longer built here.
#   0006 compilefix           follow-up to 0002.
#
# What replaces them: platform_bladerf2/bladerf2_spi.c, app_config.h, and
# AXI_ADC_NOT_PRESENT in config.h.

message(STATUS "libad936x: Build configured")
