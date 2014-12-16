cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_bootloader_recovery C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(SRC main.c ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_bootloader_recovery ${SRC})
target_link_libraries(libbladeRF_test_bootloader_recovery libbladerf_shared)

