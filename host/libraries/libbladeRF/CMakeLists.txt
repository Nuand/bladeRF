cmake_minimum_required(VERSION 3.5)
project(libbladeRF C)


################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  2)
set(VERSION_INFO_MINOR  5)
set(VERSION_INFO_PATCH  0)
set(LIBBLADERF_VERSION
  ${VERSION_INFO_MAJOR}.${VERSION_INFO_MINOR}.${VERSION_INFO_PATCH})


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

if (NOT CMAKE_INSTALL_LIBDIR)
include(GNUInstallDirs)
endif (NOT CMAKE_INSTALL_LIBDIR)

if (NOT CMAKE_INSTALL_DOCDIR)
include(GNUInstallDirs)
endif (NOT CMAKE_INSTALL_DOCDIR)

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

option(ENABLE_LIBBLADERF_SYSLOG "Enable logging to syslog (Linux/OSX)" OFF)

option(BUILD_LIBBLADERF_DOCUMENTATION "Build libbladeRF documentation. Requries Doxygen." ${BUILD_DOCUMENTATION})
if(NOT ${BUILD_DOCUMENTATION})
    set(BUILD_LIBBLADERF_DOCUMENTATION OFF)
endif()

option(BUILD_LIBBLADERF_DOC_EXAMPLES "Compile examples that are included in Doxygen documentation (mainly for QA purposes)." ${BUILD_LIBBLADERF_DOCUMENTATION})

option(ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE
      "Enable log_verbose() calls in the sync interface's data path. Note that this may harm performance."
      OFF
)

option(ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE
       "Enable log_verbose() calls on frequently-used functions in nios_access.c. Note that this may produce a lot of log output."
       OFF
)

option(ENABLE_LOCK_CHECKS
       "Enable checks for lock acquisition failures (e.g., deadlock)"
       OFF
)

option(ENABLE_USB_DEV_RESET_ON_OPEN
       "Perform a USB port reset when opening a device. This works around an issue on some Linux USB 3.0 setups that prevents a device from being opened after an application crash or unclean exit."
       ${BLADERF_OS_LINUX}
)

option(ENABLE_RFIC_TIMING_CALIBRATION
       "Perform timing calibration on the RFIC digital interface during initialization. This is only implemented on AD936x-based hardware."
       OFF
)


##############################
# Backend Support
##############################

option(ENABLE_BACKEND_USB
    "Enable USB backends. Required when using the bladeRF with a host machine."
    ON
)

option(ENABLE_BACKEND_LIBUSB
    "Enable libusb backend support."
    ${LIBUSB_FOUND}
)

option(ENABLE_BACKEND_CYAPI
    "Enable CYAPI backend support."
    ${CYAPI_FOUND}
)

option(ENABLE_BACKEND_DUMMY
    "Enable dummy backend support. This is only useful for some developers."
    OFF
)

# Ensure we've got at least one backend enabled
if(NOT ENABLE_BACKEND_LIBUSB
   AND NOT ENABLE_BACKEND_LINUX_DRIVER
   AND NOT ENABLE_BACKEND_CYAPI
   AND NOT ENABLE_BACKEND_DUMMY)
    message(FATAL_ERROR
            "Cannot enable any libbladeRF backends due to missing library support. "
            "Consider installing libusb dev package (libusb-1.0-0-dev on Ubuntu). "
            "No libbladeRF backends are enabled. "
            "Please enable one or more backends." )
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/backend_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/backend/backend_config.h
    @ONLY
)


################################################################################
# Additional compiler options
################################################################################

if(ENABLE_LIBBLADERF_LOGGING)
    add_definitions(-DLOGGING_ENABLED)
endif()

if(ENABLE_LIBBLADERF_SYSLOG)
    add_definitions(-DLOG_SYSLOG_ENABLED)
endif()

if(ENABLE_LOCK_CHECKS)
    add_definitions(-DENABLE_LOCK_CHECKS)
endif()

if(ENABLE_RFIC_TIMING_CALIBRATION)
    add_definitions(-DENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION)
endif()

if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
   ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")

    # Doing this manually instead of via add_compiler_export_flags() since
    # the GenerateExportHeader module appears to break for C-only projects:
    #
    # http://www.cmake.org/pipermail/cmake-commits/2012-August/013142.html
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()

# We define LIBBLADERF_SEARCH_PREFIX to specify where libbladeRF should look
# for files, such as FPGA bitstreams to autoload.
#
# This defaults to the value of ${CMAKE_INSTALL_PREFIX}. However,
# users may wish to override when cross compiling.
if(LIBBLADERF_SEARCH_PREFIX_OVERRIDE)
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${LIBBLADERF_SEARCH_PREFIX_OVERRIDE}")
else()
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")
endif()


################################################################################
# Include paths
################################################################################

set(LIBBLADERF_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${BLADERF_FPGA_COMMON_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src
)


################################################################################
# Build dependencies
################################################################################

if(MSVC)
    set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} ${MSVC_C99_INCLUDES})

    find_package(LibPThreadsWin32 REQUIRED)
    if(LIBPTHREADSWIN32_FOUND)
        set(HAVE_THREADS true)
        set(LIBBLADERF_INCLUDES
            ${LIBBLADERF_INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
        set(LIBBLADERF_LIBS
            ${LIBBLADERF_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
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

if(BLADERF_OS_OSX)
    set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} /usr/include/malloc)
endif()

if (NOT HAVE_THREADS)
    message(FATAL_ERROR "pthreads not found. This is required to build libbladeRF.")
endif()

if(NOT ENABLE_BACKEND_USB)
    set(ENABLE_BACKEND_LIBUSB OFF)
    set(ENABLE_BACKEND_LINUX_DRIVER OFF)
endif()

if(ENABLE_BACKEND_LIBUSB)
    if(NOT LIBUSB_FOUND)
        message(FATAL_ERROR "libusb-1.0 not found. This is required to use the libbladeRF libusb backend. For binary releases, try setting LIBUSB_PATH.")
    else(NOT LIBUSB_FOUND)
        if(LIBUSB_VERSION)
            if(NOT LIBUSB_VERSION VERSION_LESS "1.0.10")
                add_definitions(-DHAVE_LIBUSB_GET_VERSION)
            endif()

            if(WIN32)
                # We require v1.0.19 because it provides Windows 8 USB 3.0
                # speed detection fixes, additional AMD/Intel USB 3.0 root
                # hub support, and some fixes to issues reported on v1.0.18
                # that yielded corrupted samples.
                if(${LIBUSB_VERSION} VERSION_LESS "1.0.19")
                    message(FATAL_ERROR "libusb v1.0.19 is required in Windows.\n"
                                        "Please update libusb or consider using the Cypress backend if this is not possible.\n"
                                        "Detected version: ${LIBUSB_VERSION}\n")
                endif()
            elseif(APPLE)
                # A number of important changes were included in libusb
                # v1.0.16 hrough v1.0.18, including SuperSpeed support, 64-bit support,
                # and various build and crash fixes.
                if(${LIBUSB_VERSION} VERSION_LESS "1.0.18")
                    message(FATAL_ERROR "libusb v1.0.18 is required in OS X. Please update libusb."
                                        "Detected version: ${LIBUSB_VERSION}\n")
                endif()
            elseif(UNIX)
                # A number of reported issues supposedly became resolved after
                # updating to >= 1.0.16.
                if(${LIBUSB_VERSION} VERSION_LESS "1.0.16")
                    message(WARNING "\nlibusb >= 1.0.16 is HIGHLY recommended. "
                                    "If you experience issues or poor performance, please try updating libusb.\n"
                                    "Detected version: ${LIBUSB_VERSION}")
                endif()
            else()
                message(WARNING "Unexpected system type. Please report this warning to developers.")
            endif()
        elseif(BLADERF_OS_FREEBSD)
            message(STATUS "Using FreeBSD's built-in libusb implementation.")
        else()
            message(WARNING "Not checking libbladeRF/libusb compatibility because LIBUSB_VERSION is not defined.")
        endif()
        set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} ${LIBUSB_INCLUDE_DIRS})
    endif(NOT LIBUSB_FOUND)
endif(ENABLE_BACKEND_LIBUSB)

if(ENABLE_BACKEND_CYAPI)
    find_package(CyAPI REQUIRED)
    set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} ${CYAPI_INCLUDE_DIRS})
endif(ENABLE_BACKEND_CYAPI)

if(MSVC)
    set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES}
        ${LIBPTHREADSWIN32_INCLUDE_DIRS})
endif()

if(ENABLE_LIBBLADERF_SYNC_LOG_VERBOSE AND ENABLE_LIBBLADERF_LOGGING)
    add_definitions(-DENABLE_LIBBLADERF_SYNC_LOG_VERBOSE)
endif()

if(ENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE AND ENABLE_LIBBLADERF_LOGGING)
    add_definitions(-DENABLE_LIBBLADERF_NIOS_ACCESS_LOG_VERBOSE)
endif()

if(ENABLE_USB_DEV_RESET_ON_OPEN)
    add_definitions(-DENABLE_USB_DEV_RESET_ON_OPEN=1)
endif()

if(BUILD_AD936X)
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ad936x)
endif(BUILD_AD936X)

include_directories(${LIBBLADERF_INCLUDES})


################################################################################
# Configure source files
################################################################################
set(LIBBLADERF_SOURCE_BLADERF2
        ${BLADERF_FPGA_COMMON_SOURCE_DIR}/ad936x_helpers.c
        ${BLADERF_FPGA_COMMON_SOURCE_DIR}/bladerf2_common.c
        src/board/bladerf2/bladerf2.c
        src/board/bladerf2/capabilities.c
        src/board/bladerf2/common.c
        src/board/bladerf2/compatibility.c
        src/board/bladerf2/rfic_fpga.c
        src/board/bladerf2/rfic_host.c
)

set(LIBBLADERF_SOURCE
        src/backend/backend.c
        src/driver/spi_flash.c
        src/driver/fx3_fw.c
        src/driver/fpga_trigger.c
        src/driver/si5338.c
        src/driver/ina219.c
        src/driver/dac161s055.c
        src/driver/smb_clock.c
        src/board/bladerf1/bladerf1.c
        src/board/bladerf1/capabilities.c
        src/board/bladerf1/compatibility.c
        src/board/bladerf1/calibration.c
        src/board/bladerf1/flash.c
        src/board/bladerf1/image.c
        src/board/board.c
        src/expansion/xb100.c
        src/expansion/xb200.c
        src/expansion/xb300.c
        src/streaming/async.c
        src/streaming/sync.c
        src/streaming/sync_worker.c
        src/init_fini.c
        src/helpers/timeout.c
        src/helpers/file.c
        src/helpers/version.c
        src/helpers/wallclock.c
        src/helpers/interleave.c
        src/helpers/configfile.c
        src/version.h
        src/devinfo.c
        src/bladerf.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/sha256.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
        ${BLADERF_FPGA_COMMON_SOURCE_DIR}/lms.c
        ${BLADERF_FPGA_COMMON_SOURCE_DIR}/band_select.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/parse.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/range.c
)

if(BUILD_AD936X)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} ${LIBBLADERF_SOURCE_BLADERF2})
endif()

if (MSVC)
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/bladerf.rc)
endif()

if(ENABLE_BACKEND_USB)
    # Useful when doing FX3, NIOS-II, or FPGA debugging
    if(LIBBLADERF_DISABLE_USB_TIMEOUTS)
        add_definitions("-DPERIPHERAL_TIMEOUT_MS=0")
        add_definitions("-DCTRL_TIMEOUT_MS=0")
        add_definitions("-DBULK_TIMEOUT_MS=0")
    endif()

    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
        src/backend/usb/nios_access.c
        src/backend/usb/nios_legacy_access.c
        src/backend/usb/usb.c
    )
endif()

if(LIBUSB_FOUND AND ENABLE_BACKEND_LIBUSB)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/usb/libusb.c)
endif()

if(ENABLE_BACKEND_CYAPI)
    if(CYAPI_FOUND)
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/usb/cyapi.c)
        # CyAPI is C++
        set_source_files_properties(src/backend/usb/cyapi.c PROPERTIES LANGUAGE CXX)
    else()
        message(FATAL_ERROR "CyAPI was not found or is not compatible with this system.")
    endif()
endif()

if(ENABLE_BACKEND_DUMMY)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/dummy/dummy.c)
endif()

if(ENABLE_BACKEND_LINUX_DRIVER)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/linux.c)
endif()

if(BLADERF_OS_OSX)
    if (NOT HAVE_CLOCK_GETTIME)
        message(STATUS "Including clock_gettime.c shim for macOS")
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
        )
    endif()
endif()

if(MSVC)
    if (NOT HAVE_CLOCK_GETTIME)
        message(STATUS "Including clock_gettime.c shim for Windows")
        set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
        )
    endif()

    message(STATUS "Including setenv.c shim for Windows")
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/setenv.c
    )
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
    cmake_policy(SET CMP0115 OLD)
endif(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
add_library(libbladerf_shared SHARED ${LIBBLADERF_SOURCE})


################################################################################
# Build configuration
################################################################################

if(MSVC)
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
else()
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

if(ENABLE_BACKEND_LIBUSB)
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${LIBUSB_LIBRARIES})
endif()

if(ENABLE_BACKEND_CYAPI)
    set(LIBBLADERF_LIBS ${LIBBLADERF_LIBS} ${CYAPI_LIBRARIES})
endif(ENABLE_BACKEND_CYAPI)

target_link_libraries(libbladerf_shared ${LIBBLADERF_LIBS})

# Adjust our output name
set_target_properties(libbladerf_shared PROPERTIES OUTPUT_NAME bladeRF)

# Set shared library version
set_target_properties(libbladerf_shared PROPERTIES SOVERSION ${VERSION_INFO_MAJOR})

include(Shortfile)
define_file_basename_for_sources(libbladerf_shared)


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

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbladeRF.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/libdata/pkgconfig
)
else()
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbladeRF.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/
)
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")


################################################################################
# Library installation information
################################################################################

install(TARGETS libbladerf_shared
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so/.dylib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # .a/.lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR} # .dll
)

# ###############################################################################
# CMake Config Files
# ###############################################################################
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/libbladeRFConfigVersion.cmake.in
    ${PROJECT_BINARY_DIR}/bladeRFConfigVersion.cmake @ONLY)

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/libbladeRFConfig.cmake.in
    ${PROJECT_BINARY_DIR}/bladeRFConfig.cmake @ONLY)

set(cmake_files
    ${PROJECT_BINARY_DIR}/bladeRFConfig.cmake
    ${PROJECT_BINARY_DIR}/bladeRFConfigVersion.cmake)

if(UNIX)
    install(FILES ${cmake_files} DESTINATION share/cmake/bladeRF)
elseif(WIN32)
    install(FILES ${cmake_files} DESTINATION cmake)
endif()

################################################################################
# Informational output
################################################################################

message(STATUS "libbladeRF version: ${VERSION_INFO}")


################################################################################
# Doxygen documentation
################################################################################

if(BUILD_LIBBLADERF_DOC_EXAMPLES)
    add_subdirectory(doc/examples)
endif()

if(BUILD_LIBBLADERF_DOCUMENTATION)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        message(STATUS "Configured to build libbladeRF API documentation.")

        set(LOGO_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/doc/images/logo.png")

        file(GLOB INCLUDE_H ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
        file(GLOB DOC_DOX ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/*.dox)
        file(GLOB DOC_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/doc/examples/*)
        file(GLOB DOC_IMAGES ${CMAKE_CURRENT_SOURCE_DIR}/doc/images/*)
        set(DOXYGEN_SOURCE_FILES
            ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
            ${INCLUDE_H}
            ${DOC_DOX}
            ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/layout.xml
            ${DOC_EXAMPLES}
            ${DOC_IMAGES}
        )

        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
            @ONLY
        )

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/html/index.html
            DEPENDS ${DOXYGEN_SOURCE_FILES}
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen
            COMMENT "Generating libbladeRF API documentation via Doxygen in: ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen"
        )

    add_custom_target(libbladeRF-doxygen ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/html/index.html)

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/html/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/html)

    else(DOXYGEN_FOUND)
        message(FATAL_ERROR "Could not find Doxygen. Unable to build libbladeRF API documentation.")
    endif(DOXYGEN_FOUND)
endif(BUILD_LIBBLADERF_DOCUMENTATION)
