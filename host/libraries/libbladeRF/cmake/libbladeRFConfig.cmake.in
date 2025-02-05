if(DEFINED INCLUDED_LIBBLADERF_CONFIG_CMAKE)
  return()
endif()

set(INCLUDED_LIBBLADERF_CONFIG_CMAKE TRUE)

# #######################################################################
# libbladeRFConfig - cmake project configuration
#
# The following will be set after find_package(libbladeRF CONFIG):
# libbladeRF_LIBRARIES    - development libraries
# libbladeRF_INCLUDE_DIRS - development includes
# #######################################################################

# #######################################################################
# # installation root
# #######################################################################
if(UNIX)
  get_filename_component(LIBBLADERF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
elseif(WIN32)
  get_filename_component(LIBBLADERF_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

# #######################################################################
# # locate the library
# #######################################################################
find_library(
  LIBBLADERF_LIBRARY bladeRF
  PATHS ${LIBBLADERF_ROOT}/lib${LIB_SUFFIX}
  PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
  NO_DEFAULT_PATH
)

if(NOT LIBBLADERF_LIBRARY)
  message(FATAL_ERROR "cannot find libbladeRF library in ${LIBBLADERF_ROOT}/lib${LIB_SUFFIX}")
endif()

set(libbladeRF_LIBRARIES ${LIBBLADERF_LIBRARY})

# #######################################################################
# # locate the includes
# #######################################################################
find_path(
  LIBBLADERF_INCLUDE_DIR libbladeRF.h
  PATHS ${LIBBLADERF_ROOT}/include
  NO_DEFAULT_PATH
)

if(NOT LIBBLADERF_INCLUDE_DIR)
  message(FATAL_ERROR "cannot find libbladeRF includes in ${LIBBLADERF_ROOT}/include/libbladeRF")
endif()

set(libbladeRF_INCLUDE_DIRS ${LIBBLADERF_INCLUDE_DIR})
