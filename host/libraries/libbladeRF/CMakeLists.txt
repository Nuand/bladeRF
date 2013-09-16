# Copyright 2012 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
#
# Portions of this file have been borrowed from and/or inspired by
# the CMakeLists.txt from the rtl-sdr project (GPLv2)
#   http://sdr.osmocom.org/trac/wiki/rtl-sdr

cmake_minimum_required(VERSION 2.8)
project(libbladeRF C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  3)
set(VERSION_INFO_PATCH  0)
set(VERSION_INFO_EXTRA "git")
include(Version)

set(VERSION "${VERSION_INFO}")
set(LIBVER "${VERSION_INFO_BASE}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/version.h
    @ONLY
)

################################################################################
# Configuration options
################################################################################

##############################
# Backend Support
##############################

option(ENABLE_BACKEND_LIBUSB
    "Enable libusb backend support"
    ON
)

option(LIBUSB_DETACH_KERNEL_DRIVER
    "Enable calls to libusb_detach_kernel_driver(). Not supported in Windows."
    ON
)

option(ENABLE_BACKEND_LINUX_DRIVER
    "Enable Linux driver backend support."
    OFF
)

# Ensure we've got at least one backend enabled
if(NOT ENABLE_BACKEND_LIBUSB AND NOT ENABLE_BACKEND_LINUX_DRIVER)
    message(FATAL_ERROR
            "No libbladeRF backends are enabled. "
            "Please enable one or more backends." )
endif()


# Detach kernel not supported in Windows
if(CMAKE_HOST_WIN32)
    set(LIBUSB_DETACH_KERNEL_DRIVER OFF)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/backend_config.h
    @ONLY
)

################################################################################
# Additional compiler options
################################################################################
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
if(ENABLE_BACKEND_LIBUSB)
    find_package(LibUSB)
    if(NOT LIBUSB_FOUND)
        message(FATAL_ERROR "libusb-1.0 required to use for libusb backend")
    else(NOT LIBUSB_FOUND)
        set(LIBBLADERF_INCLUDES ${LIBBLADERF_INCLUDES} ${LIBUSB_INCLUDE_DIRS})
    endif(NOT LIBUSB_FOUND)
endif(ENABLE_BACKEND_LIBUSB)

if(ENABLE_BACKEND_LINUX_DRIVER)
    # TODO find Linux Driver?
endif()


include_directories(${LIBBLADERF_INCLUDES})

################################################################################
# Configure source files
################################################################################
set(LIBBLADERF_SOURCE
        src/backend.c
        src/bladerf.c
        src/bladerf_priv.c
        src/file_ops.c
        src/lms.c
        src/si5338.c
        src/version.h
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/device_identifier.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/bladerf_devinfo.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

message(STATUS "In libraries ${BLADERF_HOST_COMMON_SOURCE_DIR}")

if(LIBUSB_FOUND AND ENABLE_BACKEND_LIBUSB)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/libusb.c)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} ${BLADERF_HOST_COMMON_SOURCE_DIR}/ezusb.c)
endif()

if(ENABLE_BACKEND_LINUX_DRIVER)
    set(LIBBLADERF_SOURCE ${LIBBLADERF_SOURCE} src/backend/linux.c)
endif()

add_library(libbladerf_shared SHARED ${LIBBLADERF_SOURCE})

################################################################################
# Build configuration
################################################################################

if(ENABLE_BACKEND_LIBUSB)
    target_link_libraries(libbladerf_shared ${LIBUSB_LIBRARIES})
endif()

# Adjust our output name
set_target_properties(libbladerf_shared PROPERTIES OUTPUT_NAME bladeRF)

# Set shared library version
set_target_properties(libbladerf_shared PROPERTIES SOVERSION ${VERSION_INFO_MAJOR})


################################################################################
# Misc. installation config
################################################################################

if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR lib)
endif()

################################################################################
# Install udev rules
################################################################################
# TODO

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

if(ENABLE_BACKEND_LIBUSB)
    # TODO set(LIBBLADERF_PC_REQUIRES ${LIBBLADERF_PC_REQUIRES} libusb-1.0 = ?)
endif()

set(LIBBLADERF_PC_PREFIX ${CMAKE_INSTALL_PREFIX})
set(LIBBLADERF_PC_EXEC_PREFIX \${prefix})
set(LIBBLADERF_PC_LIBDIR \${exec_prefix}/${LIB_INSTALL_DIR})
set(LIBBLADERF_PC_INCLUDEDIR \${prefix}/include)
set(LIBBLADERF_PC_VERSION ${VERSION})

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
    DESTINATION ${LIB_INSTALL_DIR}/pkgconfig/
)

################################################################################
# Library installation information
################################################################################
install(TARGETS libbladerf_shared
    LIBRARY DESTINATION ${LIB_INSTALL_DIR} # .so/.dylib
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR} # .a/.lib
    RUNTIME DESTINATION bin                # .dll
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
find_package(Doxygen)

if(DOXYGEN_FOUND)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
    @ONLY
)

add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen
        COMMENT "Generating libbladeRF API documentation via Doxygen in: ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen"
)

endif(DOXYGEN_FOUND)
