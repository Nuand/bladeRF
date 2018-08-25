if(DEFINED __INCLUDED_BLADERF_FINDLIBEDIT_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_FINDLIBEDIT_CMAKE TRUE)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBEDIT_PKG libedit QUIET)
endif(PKG_CONFIG_FOUND)

if(NOT LIBEDIT_FOUND)
  find_path(LIBEDIT_INCLUDE_DIR NAMES histedit.h
    PATHS
    ${LIBEDIT_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBEDIT_LIBRARIES NAMES edit
    PATHS
    ${LIBEDIT_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

if(LIBEDIT_INCLUDE_DIR AND LIBEDIT_LIBRARIES)
  set(LIBEDIT_FOUND TRUE CACHE INTERNAL "libedit found")
  message(STATUS "Found libedit: ${LIBEDIT_INCLUDE_DIR}, ${LIBEDIT_LIBRARIES}")
else(LIBEDIT_INCLUDE_DIR AND LIBEDIT_LIBRARIES)
  set(LIBEDIT_FOUND FALSE CACHE INTERNAL "libedit found")
  message(STATUS "libedit not found.")
endif(LIBEDIT_INCLUDE_DIR AND LIBEDIT_LIBRARIES)

mark_as_advanced(LIBEDIT_INCLUDE_DIR LIBEDIT_LIBRARIES)

endif(NOT LIBEDIT_FOUND)
