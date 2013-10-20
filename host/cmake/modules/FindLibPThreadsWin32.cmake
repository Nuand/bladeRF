# This is file is based off of the FindLibUSB.cmake file written by Hendrik Sattler,
# from the OpenOBEX project (licensed GPLv2/LGPL).  (If this is not correct,
# please contact us so we can attribute the author appropriately.)
#
# https://github.com/zuckschwerdt/openobex/blob/master/CMakeModules/FindLibUSB.cmake
# http://dev.zuckschwerdt.org/openobex/
#
# Find pthreads-win32
#
# This requires the LIBPTHREADSWIN32_PATH variable to be set to the path to the
# a pthreads-win32 release, such as the Pre-built.2 directory from the 2.9.1 release:
#   ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip
#
# The following standard variables get defined:
#  LIBPTHREADSWIN32_FOUND:        true if LibUSB was found
#  LIBPTHREADSWIN32_HEADER_FILE:  the location of the C header file
#  LIBPTHREADSWIN32_INCLUDE_DIRS: the directories that contain headers
#  LIBPTHREADSWIN32_LIBRARIES:    the library files

if(DEFINED __INCLUDED_BLADERF_FINDLIBPTHREADSWIN32_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_FINDLIBPTHREADSWIN32_CMAKE TRUE)

include ( CheckLibraryExists )
include ( CheckIncludeFile )

set(LIBPTHREADSWIN32_PATH
        "C:/Program Files (x86)/pthreads-win32"
        CACHE
        PATH
        "Path to win-pthreads files. (This is generally only needed for Windows users who downloaded binary distributions.)"
    )

find_file ( LIBPTHREADSWIN32_HEADER_FILE
    NAMES
      pthread.h
    PATHS
      ${LIBPTHREADSWIN32_PATH}
      PATH_SUFFIXES
      include include
)
mark_as_advanced ( LIBPTHREADSWIN32_HEADER_FILE )
get_filename_component ( LIBPTHREADSWIN32_INCLUDE_DIRS "${LIBPTHREADSWIN32_HEADER_FILE}" PATH )

if ( WIN32 )
    if ( MSVC )
        if ( CMAKE_CL_64 )
            set ( LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX x64 )
        else ( CMAKE_CL_64 )
            set ( LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX x86 )
        endif ( CMAKE_CL_64 )
    elseif ( CMAKE_COMPILER_IS_GNUCC )
        if ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
            set ( LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX x64 )
        else ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
                set ( LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX x86 )
        endif ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    endif ( MSVC )

    find_library ( PTHREAD_LIBRARY
      NAMES
        pthreadVC2
      PATHS
        ${LIBPTHREADSWIN32_PATH}
        PATH_SUFFIXES
        lib/${LIBPTHREADSWIN32_LIBRARY_PATH_SUFFIX}
    )

    mark_as_advanced ( PTHREAD_LIBRARY )
    if ( PTHREAD_LIBRARY )
        set ( LIBPTHREADSWIN32_LIBRARIES ${PTHREAD_LIBRARY} )
    endif ( PTHREAD_LIBRARY )

else ( WIN32 )
    message(FATAL_ERROR "This file only supports Windows")
endif ( WIN32 )

if ( LIBPTHREADSWIN32_INCLUDE_DIRS AND LIBPTHREADSWIN32_LIBRARIES )
    set ( LIBPTHREADSWIN32_FOUND true )
endif ( LIBPTHREADSWIN32_INCLUDE_DIRS AND LIBPTHREADSWIN32_LIBRARIES )

if ( LIBPTHREADSWIN32_FOUND )
    set ( CMAKE_REQUIRED_INCLUDES "${LIBPTHREADSWIN32_INCLUDE_DIRS}" )
    check_include_file ( "{LIBPTHREADSWIN32_HEADER_FILE}" LIBPTHREADSWIN32_FOUND )
endif ( LIBPTHREADSWIN32_FOUND )

if ( NOT LIBPTHREADSWIN32_FOUND )
    message ( FATAL_ERROR "pthreads-win32 not found. If you're using a binary distribution, try setting -DLIBPTHREADSWIN32_PATH=<path_to_win_pthread_files>." )
endif ( NOT LIBPTHREADSWIN32_FOUND )
