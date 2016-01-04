# Locate the Cypress API for
#
# This module defines the following variables:
#   CYAPI_FOUND         TRUE if the Cypress API was found
#   CYAPI_HEADER_FILE   The location of the API header
#   CYAPI_INCLUDE_DIRS  The location of header files
#   CYAPI_LIBRARIES     The Cypress library files
#   CYUSB3_DRIVER_DIR   Directory containing CyUSB3 driver files
#   CYPRESS_LICENSE     Path to Cypress license shipped with FX3 SDK

if(DEFINED __INCLUDED_BLADERF_FIND_CYAPI_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_FIND_CYAPI_CMAKE TRUE)

if(NOT WIN32)
    return()
endif()

set(FX3_SDK_PATH
    "$ENV{FX3_INSTALL_PATH}"
    CACHE
    PATH
    "Path to the Cypress FX3 SDK"
)

if(NOT EXISTS "${FX3_SDK_PATH}")
    message(STATUS
            "Cypress backend not available. The following location does not exist: FX3_SDK_PATH=${FX3_SDK_PATH}")
    return()
endif()

find_file(CYAPI_HEADER_FILE
    NAMES
        CyAPI.h
    PATHS
        "${FX3_SDK_PATH}/library/cpp"
    PATH_SUFFIXES
        include inc
)
mark_as_advanced(CYAPI_HEADER_FILE)
get_filename_component(CYAPI_INCLUDE_DIRS "${CYAPI_HEADER_FILE}" PATH)

if(MSVC)
    if(CMAKE_CL_64)
        set(CYAPI_ARCH x64)
    else(CMAKE_CL_64)
        set(CYAPI_ARCH x86)
    endif(CMAKE_CL_64)
elseif(CMAKE_COMPILER_IS_GNUCC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CYAPI_ARCH x64)
    else(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CYAPI_ARCH x86)
    endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(CYUSB_ARCH "x64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
    set(CYUSB_ARCH "x86")
else()
    # We shouldn't see PPC here, as CyAPI/CYUSB3 is Windows-only
    message(FATAL_ERROR "Unexpected CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

find_library(CYAPI_LIBRARY
    NAMES
        CyAPI
    PATHS
        "${FX3_SDK_PATH}/library/cpp"
    PATH_SUFFIXES
        lib/${CYAPI_ARCH}
)
mark_as_advanced(CYAPI_LIBRARY)
if(CYAPI_LIBRARY)
    set(CYAPI_LIBRARIES "${CYAPI_LIBRARY}" SetupAPI.lib)
endif()

# Choose driver directory and INF directory (in our repo) based upon detected system
if (${CMAKE_SYSTEM_VERSION} EQUAL VERSION_5.1 OR ${CMAKE_SYSTEM_VERSION} EQUAL VERSION_5.2)
    set(CYUSB3_DRIVER_DIR       "${FX3_SDK_PATH}/driver/bin/wxp/${CYUSB_ARCH}")
    set(CYUSB3_INF              "drivers/windows/CyUSB3/winxp_vista/cyusb3.inf")
elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.0)
    set(CYUSB3_DRIVER_DIR       "${FX3_SDK_PATH}/driver/bin/vista/${CYUSB_ARCH}")
    set(CYUSB3_INF              "drivers/windows/CyUSB3/winxp_vista/cyusb3.inf")
elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.1)
    set(CYUSB3_DRIVER_DIR       "${FX3_SDK_PATH}/driver/bin/win7/${CYUSB_ARCH}")
    set(CYUSB3_INF              "drivers/windows/CyUSB3/win7_8/cyusb3.inf")
elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.2)
    set(CYUSB3_DRIVER_DIR       "${FX3_SDK_PATH}/driver/bin/win8/${CYUSB_ARCH}")
    set(CYUSB3_INF              "drivers/windows/CyUSB3/win7_8/cyusb3.inf")
elseif(${CMAKE_SYSTEM_VERSION} EQUAL 6.3)
    set(CYUSB3_DRIVER_DIR       "${FX3_SDK_PATH}/driver/bin/win81/${CYUSB_ARCH}")
    set(CYUSB3_INF              "drivers/windows/CyUSB3/win7_8/cyusb3.inf")
else()
    message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")
endif()

# CyUSB3 ships 01009 for <= Vista, and 01011 for >= Win 7
if(${CMAKE_SYSTEM_VERSION} LESS 6.1)
    set(CYUSB3_WDF_COINSTALLER  "WdfCoInstaller01009.dll")
else()
    set(CYUSB3_WDF_COINSTALLER  "WdfCoInstaller01011.dll")
endif()

set(CYPRESS_LICENSE "${FX3_SDK_PATH}/license/license.txt")

if(EXISTS ${CYUSB3_DRIVER_DIR})
    if(CYAPI_INCLUDE_DIRS AND CYAPI_LIBRARIES)
        set(CYAPI_FOUND TRUE)
    endif()
endif()


if(CYAPI_FOUND)
    set(CMAKE_REQUIRED_INCLUDES "${CYAPI_INCLUDE_DIRS}")
    check_include_file("${CYAPI_HEADER_FILE}" CYAPI_FOUND)
    message(STATUS "CyAPI includes: ${CYAPI_INCLUDE_DIRS}")
    message(STATUS "CyAPI libs: ${CYAPI_LIBRARIES}")
    message(STATUS "CYUSB3 driver directory: ${CYUSB3_DRIVER_DIR}")
endif()

if(NOT CYAPI_FOUND AND REQUIRED)
    message(FATAL_ERROR "Cypress API not found. Double-check your FX3_SDK_PATH value.")
endif()
