cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_sync C)

include_directories(${libbladeRF_SOURCE_DIR}/include)
include_directories(${libbladeRF_sync_SOURCE_DIR}/include)

set(SRC
        src/main.c
        src/test.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

add_executable(libbladeRF_test_sync ${SRC})
target_link_libraries(libbladeRF_test_sync libbladerf_shared)
