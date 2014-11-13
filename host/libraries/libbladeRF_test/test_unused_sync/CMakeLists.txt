cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_unused_sync C)

set(INCLUDES ${libbladeRF_SOURCE_DIR}/include)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES})

add_executable(libbladeRF_test_unused_sync main.c)
target_link_libraries(libbladeRF_test_unused_sync libbladerf_shared)
