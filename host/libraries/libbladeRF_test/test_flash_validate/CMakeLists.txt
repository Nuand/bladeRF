cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_flash_validate C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
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

add_definitions(-DLOGGING_ENABLED=1)

# Timed erase test uses clock_gettime(CLOCK_MONOTONIC_RAW), which is
# Linux-only. This test requires device hardware, so Windows/OSX builds
# are not a practical concern.
if(LIBC_VERSION)
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(TIMER_LIBS rt)
    endif()
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_flash_validate ${SRC})
target_link_libraries(libbladeRF_test_flash_validate libbladerf_shared ${TIMER_LIBS})
