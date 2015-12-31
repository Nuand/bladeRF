cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_version C)

set(INCLUDES ${libbladeRF_SOURCE_DIR}/include)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_version src/main.c)
target_link_libraries(libbladeRF_test_version libbladerf_shared)
