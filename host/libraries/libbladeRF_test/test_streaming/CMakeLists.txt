cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_streaming C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS})

set(SRC
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    src/main.c)

set(LIBS libbladerf_shared)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
    set(SRC ${SRC} ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c)
else()
    set(LIBS ${LIBS} m)
endif()

include_directories(${INCLUDES} ${CURL_INCLUDE_DIR} include)
add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} ${LIBS})
