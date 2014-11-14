cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_cpp CXX)

include_directories(${libbladeRF_SOURCE_DIR}/include)

add_executable(libbladeRF_test_cpp main.cpp)
target_link_libraries(libbladeRF_test_cpp libbladerf_shared)
