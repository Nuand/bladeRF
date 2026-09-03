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
set(LIBAD936X_UPSTREAM_DIR "${LIBAD936X_LOCAL_DIR}/../no-OS/drivers/rf-transceiver/ad9361")
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
    # Use no-OS repo files directly
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/util/no_os_alloc.c
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/util/no_os_util.c
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/util/no_os_mutex.c
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/api/no_os_spi.c
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/api/no_os_gpio.c
    # AXI ADC/DAC core support (now using copied files from build directory)
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/axi_adc_core.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/axi_dac_core.c
    # Only use minimal bladeRF platform layer
    ${LIBAD936X_PLATFORM_DIR}/bladerf_platform.c
    ${LIBAD936X_FPGACOMMON_DIR}/src/ad936x_params.c
)

target_compile_options(ad936x PRIVATE
    -DNUAND_MODIFICATIONS
    -DBUILD_AD936X
    -DHAVE_VERBOSE_MESSAGES
    -DHAVE_SPLIT_GAIN_TABLE=1
    -DHAVE_TDD_SYNTH_TABLE=1
    -DAD9361_DEVICE=1
    -DAD9364_DEVICE=0
    -Wall -Os
)

if(TREAT_WARNINGS_AS_ERRORS)
    target_compile_options(ad936x PRIVATE -Werror)
endif()

if(NOT MSVC)
    target_compile_options(ad936x PRIVATE
        -ffunction-sections
        -fdata-sections
        -fPIC
        -std=gnu11
    )
endif(NOT MSVC)

if(NOT MSVC)
    target_compile_options(ad936x PRIVATE
        -Wno-unused-but-set-variable
    )
endif(NOT MSVC)

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

    # Include paths for no-OS headers
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/include
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/util
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/api

    # AXI core headers
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/axi_core/axi_adc_core
    ${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/axi_core/axi_dac_core

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
    ad9361_util.c
    ad9361_util.h
    common.h
)

foreach(cpfile IN ITEMS ${LIBAD936X_UPSTREAM_FILES})
    configure_file(
        "${LIBAD936X_UPSTREAM_DIR}/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# Copy AXI ADC core files
foreach(cpfile IN ITEMS axi_adc_core.c axi_adc_core.h)
    configure_file(
        "${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/axi_core/axi_adc_core/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# Copy AXI DAC core files
foreach(cpfile IN ITEMS axi_dac_core.c axi_dac_core.h)
    configure_file(
        "${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/drivers/axi_core/axi_dac_core/${cpfile}"
        "${LIBAD936X_UPSTREAM_BUILD_DIR}/"
        COPYONLY
    )
endforeach(cpfile)

# Copy no-OS AXI IO header into build dir so we can patch it if needed
configure_file(
    "${LIBAD936X_BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS/include/no_os_axi_io.h"
    "${LIBAD936X_UPSTREAM_BUILD_DIR}/no_os_axi_io.h"
    COPYONLY
)
set(LIBAD936X_APPLY_PATCHES
    0009-fix-axi-adc-write.patch
    0010-fix-axi-adc-write.patch
    0011-axi-io-pass-userdata.patch
    0012-no-os-axi-io-ctx.patch
    0013-ad9361-init-ctx.patch
    0014-ad9361-init-signature.patch
    0015-axi-core-init-userdata.patch
    0016-fix-build-warnings.patch
    0017-preserve-bladerf2-behavior.patch
    0018-free-axi-adc.patch
)

find_package(Patcher REQUIRED)

foreach(patchfile IN ITEMS ${LIBAD936X_APPLY_PATCHES})
    message(STATUS "libad936x: Applying patch: ${patchfile}")
    execute_process(
        COMMAND ${Patcher_EXECUTABLE} -p0
        INPUT_FILE ${LIBAD936X_PATCH_DIR}/${patchfile}
        WORKING_DIRECTORY "${LIBAD936X_UPSTREAM_BUILD_DIR}"
        OUTPUT_VARIABLE outvar
        RESULT_VARIABLE resultcode
        ERROR_VARIABLE errvar
    )

    if(resultcode)
        message(FATAL_ERROR
                "Failed to apply ${patchfile}: ${errvar}\nOutput: ${outvar}")
    endif()
endforeach(patchfile)

message(STATUS "libad936x: Build configured")
