cmake_minimum_required(VERSION 2.8.5)
project(libbladeRF C)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    if(LIBPTHREADSWIN32_FOUND)
        set(HAVE_THREADS true)
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
endif(MSVC)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  14)
set(VERSION_INFO_PATCH  0)

if(NOT DEFINED VERSION_INFO_EXTRA)
    set(VERSION_INFO_EXTRA "git")
endif()
include(Version)

set(VERSION "${VERSION_INFO}")
set(LIBVER "${VERSION_INFO_BASE}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY
)

if(MSVC)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/bladerf.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/bladerf.rc
        @ONLY
    )
endif()


include(GNUInstallDirs)

# Fall back to just "lib" if the item provided by GNUInstallDirs doesn't exist
# For example, on Ubuntu 13.10 with CMake 2.8.11.2,
# /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} doesn't exist.
if (NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    message(STATUS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} does not exist. Defaulting libbladeRF install location to ${CMAKE_INSTALL_PREFIX}/lib.")
    set(CMAKE_INSTALL_LIBDIR lib)
endif()

################################################################################
# Configuration options
################################################################################

option(ENABLE_LIBBLADERF_LOGGING "Enable log messages in libbladeRF." ON)

option(BUILD_LIBBLADERF_DOCUMENTATION "Build libbladeRF documentation. Requries Doxygen." ${BUILD_DOCUMENTATION})
if(NOT ${BUILD_DOCUMENTATION})
    set(BUILD_LIBBLADERF_DOCUMENTATION OFF)
endif()

##############################
# Backend Support
##############################
option(ENABLE_BACKEND_LIBUSB "Enable libusb backend support."
    ON
)

option(ENABLE_BACKEND_DUMMY "Enable dummy backend support. This is only useful for some developers."
    OFF
)

option(LIBUSB_DETACH_KERNEL_DRIVER
    "Enable calls to libusb_detach_kernel_driver(). Not supported in Windows."
    ON
)

option(ENABLE_LIBBLADERF_SYNC
    "Enable the libbladeRF synchronous data interface. Requires pthreads."
    ON
)

option(ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
      "Enable log_verbose() calls in the sync interface's data path. Note that this may harm performance."
      OFF
)


option(ENABLE_BACKEND_LINUX_DRIVER
    "Enable Linux driver backend support."
    OFF
)

# Ensure we've got at least one backend enabled
if(NOT ENABLE_BACKEND_LIBUSB AND NOT ENABLE_BACKEND_LINUX_DRIVER AND NOT ENABLE_BACKEND_DUMMY)
    message(FATAL_ERROR
            "No libbladeRF backends are enabled. "
            "Please enable one or more backends." )
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/backend_config.h
    @ONLY
)

################################################################################
# Additional compiler options
################################################################################
if(ENABLE_LIBBLADERF_LOGGING)
    add_definitions(-DLOGGING_ENABLED)
endif()


if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )

    # Doing this manually instead of via add_compiler_export_flags() since
    # the GenerateExportHeader module appears to break for C-only projects:
    #
    # http://www.cmake.org/pipermail/cmake-commits/2012-August/013142.html
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()

################################################################################
# Include paths
################################################################################
set(LIBBLADERF_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

################################################################################
# Build dependencies
################################################################################
if(NOT HAVE_THREADS)
    message(FATAL_ERROR "Pthreads is required for libbladeRF")
endif()

if(ENABLE_BACKEND_LIBUSB)
    find_package(LibUSB)
    if(NOT LIBUSB_FOUND)
        message(FATAL_ERROR "libusb-1.0 not found. This is required to use the libbladeRF libusb backend. For binary releases, try setting LIBUSB_PATH.")
    else(NOT LIBUSB_FOUND)
        if(LIBUSB_HAVE_GET_VERSION)
            add_definitions(-DHAVE_LIBUSB_GET_VERSION)
        endif(LIBUSB_HAVE_GET_VERSION)

        set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} ${LIBUSB_INCLUDE_DIRS})
    endif(NOT LIBUSB_FOUND)
endif(ENABLE_BACKEND_LIBUSB)

if(ENABLE_LIBBLADERF_SYNC)
    if(MSVC)
        set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES}
            ${LIBPTHREADSWIN32_INCLUDE_DIRS})
    endif()

    add_definitions(-DENABLE_LIBBLADERF_SYNC)

    if(ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE AND ENABLE_LIBBLADERF_LOGGING)
        add_definitions(-DENABLE_LIBBLADERF_SYNC_LOG_VERBOSE)
    endif()
endif(ENABLE_LIBBLADERF_SYNC)

include_directories(${LIBBLADERF_INCLUDES})

################################################################################
# Configure source files
################################################################################
set(LIBBLADERF_SOURCE
        src/async.c
        src/backend.c
        src/bladerf.c
        src/bladerf_priv.c
        src/file_ops.c
        src/lms.c
        src/si5338.c
        src/version.h
        src/device_identifier.c
        src/devinfo.c
        src/flash.c
        src/image.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/sha256.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

if (MSVC)
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/bladerf.rc)
endif()

if(LIBUSB_FOUND AND ENABLE_BACKEND_LIBUSB)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/libusb.c)
endif()

if(ENABLE_BACKEND_DUMMY)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/dummy.c)
endif()

if(ENABLE_BACKEND_LINUX_DRIVER)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/linux.c)
endif()

if(ENABLE_LIBBLADERF_SYNC)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
        src/sync.c
        src/sync_worker.c
    )

    if(BLADERF_OS_OSX)
       set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
           ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
       )
    endif()

    if(MSVC)
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
        )
    endif()
endif()

add_library(libbladerf_shared SHARED ${LIBBLADERF_SOURCE})

################################################################################
# Build configuration
################################################################################

if(ENABLE_LIBBLADERF_SYNC)
    if(MSVC)
        set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
    else()
        set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${CMAKE_THREAD_LIBS_INIT})
    endif(MSVC)
endif()

if(ENABLE_BACKEND_LIBUSB)
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${LIBUSB_LIBRARIES})
endif()

target_link_libraries(libbladerf_shared ${LIBBLADERF_LIBS})

# Adjust our output name
set_target_properties(libbladerf_shared PROPERTIES OUTPUT_NAME bladeRF)

# Set shared library version
set_target_properties(libbladerf_shared PROPERTIES SOVERSION ${VERSION_INFO_MAJOR})


################################################################################
# Generate pkg-config file
################################################################################
add_subdirectory(include)
foreach(inc ${LIBBLADERF_INCLUDE_DIR})
    list(APPEND LIBBLADERF_PC_CFLAGS "-I${inc}")
endforeach()

foreach(lib ${LIBBLADERF_LIBRARY_DIRS})
    list(APPEND LIBBLADERF_PC_PRIV_LIBS "-L${lib}")
endforeach()

set(LIBBLADERF_PC_PREFIX ${CMAKE_INSTALL_PREFIX})
set(LIBBLADERF_PC_EXEC_PREFIX \${prefix})
set(LIBBLADERF_PC_LIBDIR \${exec_prefix}/${CMAKE_INSTALL_LIBDIR})
set(LIBBLADERF_PC_INCLUDEDIR \${prefix}/include)
set(LIBBLADERF_PC_VERSION ${VERSION})
set(LIBBLADERF_PC_LIBS "-lbladeRF")

# Use space-delimiter in the .pc file, rather than CMake's semicolon separator
string(REPLACE ";" " " LIBBLADERF_PC_CFLAGS "${LIBBLADERF_PC_CFLAGS}")
string(REPLACE ";" " " LIBBLADERF_PC_LIBS "${LIBBLADERF_PC_LIBS}")

# Unset these to avoid hard-coded paths in a cross-environment
if(CMAKE_CROSSCOMPILING)
    unset(LIBBLADERF_PC_CFLAGS)
    unset(LIBBLADERF_PC_LIBS)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libbladeRF.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libbladeRF.pc
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbladeRF.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/
)

################################################################################
# Library installation information
################################################################################
install(TARGETS libbladerf_shared
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so/.dylib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # .a/.lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR} # .dll
)


################################################################################
# Informational output
################################################################################
message(STATUS "libbladeRF version: ${VERSION_INFO}")

################################################################################
# Library tests
################################################################################
add_subdirectory(tests)

################################################################################
# Doxygen documentation
################################################################################
if(BUILD_LIBBLADERF_DOCUMENTATION)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        message(STATUS "Configured to build libbladeRF API documentation.")
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
            @ONLY
            )

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/html/index.html
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile ${CMAKE_CURRENT_SOURCE_DIR}/include/libbladeRF.h
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen
            COMMENT "Generating libbladeRF API documentation via Doxygen in: ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen"
        )

    add_custom_target(libbladeRF-doxygen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/html/index.html)

    else(DOXYGEN_FOUND)
        message(WARNING "Could not find Doxygen. libbladeRF API documentation will not be built.")
    endif(DOXYGEN_FOUND)
endif(BUILD_LIBBLADERF_DOCUMENTATION)
