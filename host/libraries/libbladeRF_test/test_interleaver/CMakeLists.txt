cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_interleaver C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

add_definitions(-DLOGGING_ENABLED=1)

if(LIBBLADERF_SEARCH_PREFIX_OVERRIDE)
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${LIBBLADERF_SEARCH_PREFIX_OVERRIDE}")
else()
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")
endif()

set(SRC
    src/main.c
    ${libbladeRF_SOURCE_DIR}/src/helpers/interleave.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_interleaver ${SRC})
target_link_libraries(libbladeRF_test_interleaver libbladerf_shared)
