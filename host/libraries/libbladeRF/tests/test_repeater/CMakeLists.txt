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

if(MSVC)
    set(TEST_REPEATER_SRC ${TEST_REPEATER_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
       )
endif()

set(TEST_REPEATER_INCLUDE
        src
        ${libbladeRF_SOURCE_DIR}/include
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(MSVC)
    set(TEST_REPEATER_INCLUDE ${TEST_REPEATER_INCLUDE} 
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/windows
        ${LIBPTHREADSWIN32_INCLUDE_DIRS}
    )
endif()


include_directories(${TEST_REPEATER_INCLUDE})
add_executable(libbladeRF_test_repeater ${TEST_REPEATER_SRC})

set(TEST_REPEATER_LIBS
        libbladerf_shared
        ${CMAKE_THREAD_LIBS_INIT})


if(LIBPTHREADSWIN32_FOUND)
    set(TEST_REPEATER_LIBS ${TEST_REPEATER_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

target_link_libraries(libbladeRF_test_repeater
    ${TEST_REPEATER_LIBS})
