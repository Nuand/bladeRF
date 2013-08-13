# This file has been borrowed from the gr-osmosdr project (GPLv3)
# Author: Dimitri Stolnikov 
#
#   http://openbsc.osmocom.org/trac/wiki/OsmocomOverview
#   http://git.osmocom.org/gr-osmosdr/log/cmake/Modules/FindLibbladeRF.cmake
#
if(NOT LIBBLADERF_FOUND)
  pkg_check_modules (LIBBLADERF_PKG libbladeRF)
  find_path(LIBBLADERF_INCLUDE_DIR NAMES libbladeRF.h
    PATHS
    ${LIBBLADERF_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBBLADERF_LIBRARIES NAMES bladeRF
    PATHS
    ${LIBBLADERF_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

if(LIBBLADERF_INCLUDE_DIR AND LIBBLADERF_LIBRARIES)
  set(LIBBLADERF_FOUND TRUE CACHE INTERNAL "libbladeRF found")
  message(STATUS "Found libbladeRF: ${LIBBLADERF_INCLUDE_DIR}, ${LIBBLADERF_LIBRARIES}")
else(LIBBLADERF_INCLUDE_DIR AND LIBBLADERF_LIBRARIES)
  set(LIBBLADERF_FOUND FALSE CACHE INTERNAL "libbladeRF found")
  message(STATUS "libbladeRF not found.")
endif(LIBBLADERF_INCLUDE_DIR AND LIBBLADERF_LIBRARIES)

mark_as_advanced(LIBBLADERF_INCLUDE_DIR LIBBLADERF_LIBRARIES)

endif(NOT LIBBLADERF_FOUND) 

