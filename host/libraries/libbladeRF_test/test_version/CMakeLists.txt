cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_version C)

set(INCLUDES ${libbladeRF_SOURCE_DIR}/include)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_version src/main.c)
target_link_libraries(libbladeRF_test_version libbladerf_shared)
