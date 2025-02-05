cmake_minimum_required(VERSION 3.5)
add_subdirectory(libbladeRF)

# Conditionally add the test directory
option(TEST_LIBBLADERF "Enables libbladeRF test output" ON)
if(TEST_LIBBLADERF)
    message(STATUS "libbladeRF_test: enabled")
    add_subdirectory(libbladeRF_test)
else()
    message(STATUS "libbladeRF_test: disabled")
endif()
