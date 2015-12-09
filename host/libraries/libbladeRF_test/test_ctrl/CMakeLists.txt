cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_ctrl C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
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
        src/test_bandwidth.c
        src/test_correction.c
        src/test_enable_module.c
        src/test_frequency.c
        src/test_gain.c
        src/test_loopback.c
        src/test_rx_mux.c
        src/test_sampling.c
        src/test_lpf_mode.c
        src/test_samplerate.c
        src/test_threads.c
        src/test_xb200.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_ctrl ${SRC})
target_link_libraries(libbladeRF_test_ctrl ${LIBS})
