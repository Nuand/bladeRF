cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_repeater C)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
else(MSVC)
	find_package(Threads REQUIRED)
endif(MSVC)

set(TEST_REPEATER_SRC
        src/main.c
        src/repeater.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

set(TEST_REPEATER_INCLUDE
        src
        ${libbladeRF_SOURCE_DIR}/include
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)



include_directories(${TEST_REPEATER_INCLUDE})
add_executable(libbladeRF_test_repeater ${TEST_REPEATER_SRC})

target_link_libraries(libbladeRF_test_repeater
                        libbladerf_shared
                        ${CMAKE_THREAD_LIBS_INIT}
)
