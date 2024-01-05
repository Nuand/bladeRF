cmake_minimum_required(VERSION 3.5)
project(libbladeRF_test_fpga_load C)

set(INCLUDES ${libbladeRF_SOURCE_DIR}/include)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES} ${CURL_INCLUDE_DIR} include)
add_executable(${PROJECT_NAME} src/main.c)

target_link_libraries(${PROJECT_NAME} libbladerf_shared)
