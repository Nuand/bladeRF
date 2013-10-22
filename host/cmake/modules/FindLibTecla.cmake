if(DEFINED __INCLUDED_BLADERF_FINDLIBTECLA_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_FINDLIBTECLA_CMAKE TRUE)

find_package(PkgConfig)

if(NOT LIBTECLA_FOUND)
  pkg_check_modules (LIBTECLA_PKG libtecla)
  find_path(LIBTECLA_INCLUDE_DIR NAMES libtecla.h
    PATHS
    ${LIBTECLA_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBTECLA_LIBRARIES NAMES tecla
    PATHS
    ${LIBTECLA_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

if(LIBTECLA_INCLUDE_DIR AND LIBTECLA_LIBRARIES)
  set(LIBTECLA_FOUND TRUE CACHE INTERNAL "libtecla found")
  message(STATUS "Found libtecla: ${LIBTECLA_INCLUDE_DIR}, ${LIBTECLA_LIBRARIES}")
else(LIBTECLA_INCLUDE_DIR AND LIBTECLA_LIBRARIES)
  set(LIBTECLA_FOUND FALSE CACHE INTERNAL "libtecla found")
  message(STATUS "libtecla not found.")
endif(LIBTECLA_INCLUDE_DIR AND LIBTECLA_LIBRARIES)

mark_as_advanced(LIBTECLA_INCLUDE_DIR LIBTECLA_LIBRARIES)

endif(NOT LIBTECLA_FOUND) 
