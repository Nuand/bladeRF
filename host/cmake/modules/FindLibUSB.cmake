# This is a modified version of the file written by Hedrik Sattler,
# from the OpenOBEX project (licensed GPLv2/LGPL).  (If this is not correct,
# please contact us so we can attribute the author appropriately.)
#
# https://github.com/zuckschwerdt/openobex/blob/master/CMakeModules/FindLibUSB.cmake
# http://dev.zuckschwerdt.org/openobex/
#
# Find libusb(x)-1.0
#
# It will use PkgConfig if present and supported, otherwise this
# script searches for binary distribution in the path defined by
# the LIBUSB_PATH variable.
#
# The following standard variables get defined:
#  LIBUSB_FOUND:        true if LibUSB was found
#  LIBUSB_HEADER_FILE:  the location of the C header file
#  LIBUSB_INCLUDE_DIRS: the directorys that contain headers
#  LIBUSB_LIBRARIES:    the library files
#  LIBUSB_HAS_HOTPLUG   the library provides hotplug support
#
#  Set LIBUSB_SUPPRESS_WARNINGS to suppress warnings from this file

if(DEFINED __INCLUDED_BLADERF_FINDLIBUSB_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_FINDLIBUSB_CMAKE TRUE)

include(CheckLibraryExists)
include(CheckIncludeFile)


# In Linux, folks should generally be able to simply fetch the libusb library and
# development packages from their distros package repository. Windows users will
# likely want to fetch a binary distribution, hence the Windows-oriented default.
#
# See http://www.libusb.org/wiki/windows_backend#LatestBinarySnapshots
set(LIBUSB_PATH
    "C:/Program Files (x86)/libusbx-1.0.16"
    CACHE
    PATH
    "Path to libusb files. (This is generally only needed for Windows users who downloaded binary distributions.)"
   )

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PKGCONFIG_LIBUSB libusb-1.0)
else(PKG_CONFIG_FOUND)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        if(NOT LIBUSB_SUPPRESS_WARNINGS)
            message(WARNING "Could not find pkg-config, which is a build dependency. If libusb-1.0 can't be found, please install pkg-config.")
        endif(NOT LIBUSB_SUPPRESS_WARNINGS)
    endif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
endif(PKG_CONFIG_FOUND)

if(PKGCONFIG_LIBUSB_FOUND)
    set(LIBUSB_INCLUDE_DIRS ${PKGCONFIG_LIBUSB_INCLUDE_DIRS})
    foreach(i ${PKGCONFIG_LIBUSB_LIBRARIES})
        string(REGEX MATCH "[^-]*" ibase "${i}")
        find_library(${ibase}_LIBRARY
            NAMES ${i}
            PATHS ${PKGCONFIG_LIBUSB_LIBRARY_DIRS}
           )
        if(${ibase}_LIBRARY)
            list(APPEND LIBUSB_LIBRARIES ${${ibase}_LIBRARY})
        endif(${ibase}_LIBRARY)
        mark_as_advanced(${ibase}_LIBRARY)
    endforeach(i)

else(PKGCONFIG_LIBUSB_FOUND)
    find_file(LIBUSB_HEADER_FILE
        NAMES
        libusb.h
        PATHS
        ${LIBUSB_PATH}
        PATH_SUFFIXES
        include include/libusbx-1.0 include/libusb-1.0
       )
    mark_as_advanced(LIBUSB_HEADER_FILE)
    get_filename_component(LIBUSB_INCLUDE_DIRS "${LIBUSB_HEADER_FILE}" PATH)

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        # The libusbx binary distribution contains several libs.
        # Use the lib that got compiled with the same compiler.
        if(MSVC)
            if(CMAKE_CL_64)
                set(LIBUSB_LIBRARY_PATH_SUFFIX MS64/dll)
            else(CMAKE_CL_64)
                set(LIBUSB_LIBRARY_PATH_SUFFIX MS32/dll)
            endif(CMAKE_CL_64)
        elseif(CMAKE_COMPILER_IS_GNUCC)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(LIBUSB_LIBRARY_PATH_SUFFIX MinGW32/dll)
            else(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(LIBUSB_LIBRARY_PATH_SUFFIX MinGW64/dll)
            endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        endif(MSVC)
    endif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

    find_library(usb_LIBRARY
        NAMES
        libusb-1.0
        PATHS
        ${LIBUSB_PATH}
        PATH_SUFFIXES
        ${LIBUSB_LIBRARY_PATH_SUFFIX}
       )
    mark_as_advanced(usb_LIBRARY)
    if(usb_LIBRARY)
        set(LIBUSB_LIBRARIES ${usb_LIBRARY})
    endif(usb_LIBRARY)

endif(PKGCONFIG_LIBUSB_FOUND)

if(LIBUSB_INCLUDE_DIRS AND LIBUSB_LIBRARIES)
    set(LIBUSB_FOUND true)
endif(LIBUSB_INCLUDE_DIRS AND LIBUSB_LIBRARIES)

if(LIBUSB_FOUND)
    set(CMAKE_REQUIRED_INCLUDES "${LIBUSB_INCLUDE_DIRS}")
    check_include_file("{LIBUSB_HEADER_FILE}" LIBUSB_FOUND)
endif(LIBUSB_FOUND)

if(LIBUSB_FOUND)

    # Introduced in v1.0.10
    check_library_exists("${usb_LIBRARY}" libusb_get_version "" LIBUSB_HAVE_GET_VERSION)

    # Introduced in 1.0.16
    check_library_exists("${usb_LIBRARY}" libusb_strerror "" LIBUSB_HAVE_STRERROR)
    if (NOT LIBUSB_HAVE_STRERROR)
        if(NOT LIBUSB_SUPPRESS_WARNINGS)
            message(WARNING "Detected libusb < 1.0.16. For best results, consider updating to a more recent libusb version.")
        endif(NOT LIBUSB_SUPPRESS_WARNINGS)
    endif(NOT LIBUSB_HAVE_STRERROR)

    # Provide a hook to check it hotplug support is provided (1.0.16)
    check_library_exists("${usb_LIBRARY}" libusb_hotplug_register_callback  "" LIBUSB_HAVE_HOTPLUG)

endif(LIBUSB_FOUND)
