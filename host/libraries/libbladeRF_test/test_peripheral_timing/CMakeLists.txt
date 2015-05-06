# This program currently uses clock_gettime(CLOCK_MONOTONIC_RAW), which
# does not appear to be supported on Windows or OSX.
#
# Given that this is really only intended for use as a benchmark for changes to
# the NIOS II code, it probably won't get much general use, so we'll just
# leave this as-is for now.
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    cmake_minimum_required(VERSION 2.8)
    project(libbladeRF_test_peripheral_timing C)

    set(INCLUDES ${libbladeRF_SOURCE_DIR}/include)
        set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})

    include_directories(${INCLUDES})
    add_executable(libbladeRF_test_peripheral_timing main.c)
    target_link_libraries(libbladeRF_test_peripheral_timing libbladerf_shared)
endif()
