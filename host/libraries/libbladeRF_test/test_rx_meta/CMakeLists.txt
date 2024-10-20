project(libbladeRF_test_rx_meta
        VERSION 2.8
        LANGUAGES C)

set(INCLUDES
    ./include
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)
include_directories(${INCLUDES})

set(SRC
    ./src/main.c
    ./src/include.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    )
if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif(MSVC)
add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(
    ${PROJECT_NAME}
    libbladerf_shared
)
