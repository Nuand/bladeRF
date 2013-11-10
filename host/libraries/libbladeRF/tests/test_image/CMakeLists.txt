include_directories(${libbladeRF_SOURCE_DIR}/include)

add_executable(libbladeRF_test_image main.c)
target_link_libraries(libbladeRF_test_image libbladerf_shared)
