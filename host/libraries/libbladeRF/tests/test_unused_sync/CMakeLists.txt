cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_unused_sync C)

include_directories(${libbladeRF_SOURCE_DIR}/include)

add_executable(libbladeRF_test_unused_sync main.c)
target_link_libraries(libbladeRF_test_unused_sync libbladerf_shared)
