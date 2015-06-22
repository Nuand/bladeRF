cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_freq_hop C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

set(LIBS libbladerf_shared)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
    find_package(LibPThreadsWin32 REQUIRED)
    set(INCLUDES ${INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
    set(LIBS ${LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

set(SRC
    main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/test_common.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/devcfg.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_freq_hop ${SRC})
target_link_libraries(libbladeRF_test_freq_hop ${LIBS})
