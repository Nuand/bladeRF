cmake_minimum_required(VERSION 3.5)
project(libbladeRF_test_gain_compare C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libbladeRF/src/streaming
)

set(LIBS libbladerf_shared)

add_definitions(-DLOGGING_ENABLED=1)
set(SRC
        src/main.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_gain_compare ${SRC})
target_link_libraries(libbladeRF_test_gain_compare ${LIBS})

