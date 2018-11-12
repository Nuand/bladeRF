cmake_minimum_required(VERSION 2.8.5)
project(ad936x C)

message(STATUS "libad936x: Configuring build...")

################################################################################
# Options
################################################################################
option(LIBAD936X_DEBUG
       "Enable debugging output."
       OFF)

################################################################################
# Paths
################################################################################
# Source directory for local files
set(LIBAD936X_LOCAL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# Repository root
set(LIBAD936X_BLADERF_ROOT ${LIBAD936X_LOCAL_DIR}/../../../)
# libbladeRF relative path
set(LIBAD936X_LIBBLADERF_DIR ${LIBAD936X_BLADERF_ROOT}/host/libraries/libbladeRF/)

# Patch files live here
set(LIBAD936X_PATCH_DIR ${LIBAD936X_LOCAL_DIR}/patches)
# Platform files
set(LIBAD936X_PLATFORM_DIR ${LIBAD936X_LOCAL_DIR}/platform_bladerf2)

# Upstream source (clean)
set(LIBAD936X_UPSTREAM_DIR ${LIBAD936X_LOCAL_DIR}/../no-OS/ad9361/sw)
# Upstream source (patched)
set(LIBAD936X_UPSTREAM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

################################################################################
# Dependencies
################################################################################
set(LIBAD936X_INCLUDES
    ${LIBAD936X_PLATFORM_DIR}
    ${LIBAD936X_UPSTREAM_BUILD_DIR}
    ${LIBAD936X_LIBBLADERF_DIR}/src
    ${LIBAD936X_LIBBLADERF_DIR}/include

    ${CMAKE_CURRENT_SOURCE_DIR}/../../../host/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware_common

    # host_config.h, two possible places
    ${CMAKE_CURRENT_BINARY_DIR}/../../../host/common/include
    ${CMAKE_CURRENT_BINARY_DIR}/../../../common/include
)

if(MSVC)
    set(LIBAD936X_INCLUDES ${LIBAD936X_INCLUDES} ${MSVC_C99_INCLUDES})

    find_package(LibPThreadsWin32 REQUIRED)
    if(LIBPTHREADSWIN32_FOUND)
        set(HAVE_THREADS true)
        set(LIBAD936X_INCLUDES
            ${LIBAD936X_INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
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
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(LIBAD936X_INCLUDES ${LIBAD936X_INCLUDES} /usr/include/malloc)
endif()

# Work around cross-platform case sensitivity issues
find_package(Patch QUIET)
if(PATCH_FOUND)
    if (PATCH_EXECUTABLE)
        set(Patch_FOUND ${PATCH_FOUND})
        set(Patch_EXECUTABLE ${PATCH_EXECUTABLE})
    endif()
endif()

find_package(Git QUIET)
if(GIT_FOUND)
    if (GIT_EXECUTABLE)
        set(Git_FOUND ${GIT_FOUND})
        set(Git_EXECUTABLE ${GIT_EXECUTABLE})
    endif()
endif()

# Try to figure out how we're going to patch this library.
# patch is preferred, but git-apply is also okay.
if(Patch_FOUND)
    message(STATUS "libad936x: Will use ${Patch_EXECUTABLE} for patching")
    set(PATCH_CMD ${Patch_EXECUTABLE} -p3 -i)
else()
    if(Git_FOUND)
        message(STATUS "libad936x: Will use ${Git_EXECUTABLE} for patching")
        set(PATCH_CMD ${Git_EXECUTABLE} --bare apply -p3)
    else()
        message(FATAL_ERROR "Could not find either 'patch' or 'git'!")
    endif()
endif()

# Submodules require additional effort
if(NOT IS_DIRECTORY ${LIBAD936X_UPSTREAM_DIR})
    if(Git_FOUND)
        message(STATUS "libad936x: Attempting to initialize missing submodule")
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

# Re-check for submodule...
if(NOT IS_DIRECTORY ${LIBAD936X_UPSTREAM_DIR})
    message(FATAL_ERROR "Directory ${LIBAD936X_UPSTREAM_DIR} was not found. "
                        "It appears that this submodule has not been properly "
                        "initialized. Please run 'git submodule init' and "
                        "'git submodule update', and then re-run cmake.")
endif()

################################################################################
# Apply patches
################################################################################
# All of the patch files to apply
set(LIBAD936X_PATCHES
    0001-0bba46e-nuand-modifications.patch
    0002-0bba46e-pr-561.patch
    0003-0bba46e-pr-573.patch
    0004-0bba46e-pr-598.patch
)

# All of the files that we need from the upstream repository
set(LIBAD936X_UPSTREAM_FILES
    ad9361.c
    ad9361.h
    ad9361_api.c
    ad9361_api.h
    ad9361_conv.c
    common.h
    config.h
    util.c
    util.h
)

foreach(cpfile IN ITEMS ${LIBAD936X_UPSTREAM_FILES})
    set(cpfile_full ${LIBAD936X_UPSTREAM_DIR}/${cpfile})
    if(NOT EXISTS ${cpfile_full})
        message(FATAL_ERROR "Could not find required file: ${cpfile_full}")
    endif()
    set(cptarget ${LIBAD936X_UPSTREAM_BUILD_DIR}/)
    configure_file(${cpfile_full} ${cptarget} COPYONLY)
endforeach(cpfile)

foreach(patchfile IN ITEMS ${LIBAD936X_PATCHES})
    message(STATUS "libad936x: Applying patch: ${patchfile}")
    execute_process(
        COMMAND ${PATCH_CMD} ${LIBAD936X_PATCH_DIR}/${patchfile}
        WORKING_DIRECTORY ${LIBAD936X_UPSTREAM_BUILD_DIR}
        OUTPUT_QUIET
    )
endforeach(patchfile)

################################################################################
# Configure build
################################################################################
set(LIBAD936X_SOURCE
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_api.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/ad9361_conv.c
    ${LIBAD936X_UPSTREAM_BUILD_DIR}/util.c
    ${LIBAD936X_PLATFORM_DIR}/platform.c
    ${LIBAD936X_PLATFORM_DIR}/adc_core.c
    ${LIBAD936X_PLATFORM_DIR}/dac_core.c
    ${LIBAD936X_PLATFORM_DIR}/params.c
)

if(NOT MSVC)
    set_source_files_properties(${LIBAD936X_SOURCE}
        PROPERTIES COMPILE_FLAGS "-std=gnu11")
endif(NOT MSVC)

include_directories(${LIBAD936X_INCLUDES})
add_library(ad936x STATIC ${LIBAD936X_SOURCE})
target_link_libraries(ad936x ${LIBAD936X_LIBS})

# Clean out existing compiler options
set_target_properties(ad936x PROPERTIES COMPILE_OPTIONS "")
target_compile_options(ad936x PRIVATE -DNUAND_MODIFICATIONS -Wall -Os)

if(NOT MSVC)
    target_compile_options(ad936x PRIVATE
        -ffunction-sections -fdata-sections -fPIC -Wno-unused-but-set-variable)
endif(NOT MSVC)

if(BUILD_AD936X_DEBUG)
    message(STATUS "libad936x: Enabling debugging output")
    target_compile_options(ad936x PRIVATE -D_DEBUG)
endif(BUILD_AD936X_DEBUG)

message(STATUS "libad936x: Build configured")
