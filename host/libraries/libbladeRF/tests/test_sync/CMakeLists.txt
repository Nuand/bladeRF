cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_sync C)

set(TEST_SYNC_INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_sync_SOURCE_DIR}/include
)

set(TEST_SYNC_LIBS libbladerf_shared)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    set(TEST_SYNC_INCLUDES ${TEST_SYNC_INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
    set(TEST_SYNC_LIBS ${TEST_SYNC_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
else(MSVC)
    find_package(Threads REQUIRED)
    set(TEST_SYNC_LIBS ${TEST_SYNC_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

set(SRC
        src/main.c
        src/test.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

add_executable(libbladeRF_test_sync ${SRC})
target_link_libraries(libbladeRF_test_sync ${TEST_SYNC_LIBS})
