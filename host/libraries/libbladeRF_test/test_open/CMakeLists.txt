cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_open C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(SRC src/main.c ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_open ${SRC})
target_link_libraries(libbladeRF_test_open libbladerf_shared)

