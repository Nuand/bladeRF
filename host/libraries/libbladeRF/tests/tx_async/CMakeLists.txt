cmake_minimum_required(VERSION 2.8)
project(tx_async C)

include_directories(${libbladeRF_SOURCE_DIR}/include)

add_executable(tx_async main.c)
target_link_libraries(tx_async libbladerf_shared)
