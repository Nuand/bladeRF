cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_scheduled_retune C)

include_directories(${libbladeRF_SOURCE_DIR}/include)

set(INCLUDES
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(LIBS libbladerf_shared)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    set(INCLUDES ${INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
    set(LIBS ${LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
else(MSVC)
    find_package(Threads REQUIRED)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

set(SRC
    src/main.c
    src/hop_set.c
    ../common/src/test_common.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/devcfg.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif(MSVC)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_scheduled_retune ${SRC})
target_link_libraries(libbladeRF_test_scheduled_retune ${LIBS})
