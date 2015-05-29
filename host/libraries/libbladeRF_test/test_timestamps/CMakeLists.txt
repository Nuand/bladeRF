cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_timestamps C)

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
else()
    find_package(Threads REQUIRED)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)


set(SRC
    src/main.c
    src/test_timestamps.c
    src/test_rx_gaps.c
    src/test_rx_scheduled.c
    src/test_tx_onoff.c
    src/test_tx_onoff_nowsched.c
    src/test_tx_gmsk_bursts.c
    src/test_loopback_onoff.c
    src/test_loopback_onoff_zp.c
    src/loopback.c
    src/test_format_mismatch.c
    src/test_readback.c
    src/test_print_timestamps.c
    ../common/src/test_common.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif(MSVC)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_timestamps ${SRC})
target_link_libraries(libbladeRF_test_timestamps ${LIBS})
