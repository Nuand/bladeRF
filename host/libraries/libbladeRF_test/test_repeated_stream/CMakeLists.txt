cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_repeated_stream C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

set(SRC
    src/main.c 
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/devcfg.c
)

set(LIBS libbladerf_shared)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    set(INCLUDES ${INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS} ${MSVC_C99_INCLUDES})
    set(LIBS ${LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
    set(SRC ${SRC} ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c)
else(MSVC)
    find_package(Threads REQUIRED)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_repeated_stream ${SRC})
target_link_libraries(libbladeRF_test_repeated_stream ${LIBS})
