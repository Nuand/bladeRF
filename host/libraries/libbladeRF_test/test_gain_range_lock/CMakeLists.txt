cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_gain_range_lock C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

include_directories(${INCLUDES})

find_package(Threads REQUIRED)

add_executable(libbladeRF_test_gain_range_lock main.c)
target_link_libraries(libbladeRF_test_gain_range_lock
    libbladerf_shared
    Threads::Threads
)
