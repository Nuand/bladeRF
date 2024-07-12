cmake_minimum_required(VERSION 3.5)
project(libbladeRF_test_streaming C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(SRC
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    src/main.c)

include_directories(${INCLUDES} ${CURL_INCLUDE_DIR} include)
add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} libbladerf_shared m)
