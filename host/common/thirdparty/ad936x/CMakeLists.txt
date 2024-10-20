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
set(LIBAD936X_UPSTREAM_DIR "${LIBAD936X_LOCAL_DIR}/../no-OS/ad9361/sw")
# Upstream source (patched)
set(LIBAD936X_UPSTREAM_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")


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

    find_package(LibPThreadsWin32 REQUIRED)
    if(LIBPTHREADSWIN32_FOUND)
        set(HAVE_THREADS true)
        target_include_directories(ad936x PUBLIC ${LIBPTHREADSWIN32_INCLUDE_DIRS})
        set(LIBAD936X_LIBS
            ${LIBAD936X_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
    else()
        set(HAVE_THREADS false)
    endif()
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
