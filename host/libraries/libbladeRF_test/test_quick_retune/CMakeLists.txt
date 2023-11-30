cmake_minimum_required(VERSION 3.5)
project(libbladeRF_test_quick_retune C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(LIBS libbladerf_shared)

set(SRC
        main.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)
if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_quick_retune ${SRC})
target_link_libraries(libbladeRF_test_quick_retune ${LIBS})
