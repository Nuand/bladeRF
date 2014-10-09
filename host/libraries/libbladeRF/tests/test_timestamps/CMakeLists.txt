cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_timestamps C)

include_directories(${libbladeRF_SOURCE_DIR}/include)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/include
)

set(LIBS libbladerf_shared)

if(MSVC)
    set(LIBS ${LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
else()
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

include_directories(${INCLUDES})

set(SRC
    src/main.c
    src/test_timestamps.c
    src/test_rx_gaps.c
    src/test_rx_scheduled.c
    src/test_tx_onoff.c
    src/test_tx_gmsk_bursts.c
    src/test_loopback_onoff.c
    src/test_format_mismatch.c
    src/test_readback.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif(MSVC)

add_executable(libbladeRF_test_timestamps ${SRC})
target_link_libraries(libbladeRF_test_timestamps ${LIBS})
