cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_fpga_load C)

set(INCLUDES ${libbladeRF_SOURCE_DIR}/include ${BLADERF_HOST_COMMON_INCLUDE_DIRS})
set(SRC src/main.c)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
    set(SRC ${SRC} ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c)
endif()

include_directories(${INCLUDES} include)
add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} libbladerf_shared)
