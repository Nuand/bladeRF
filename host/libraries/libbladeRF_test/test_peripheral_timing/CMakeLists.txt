# This program currently uses clock_gettime(CLOCK_MONOTONIC_RAW), which
# does not appear to be supported on Windows or OSX.
#
# Given that this is really only intended for use as a benchmark for changes to
# the NIOS II code, it probably won't get much general use, so we'll just
# leave this as-is for now.
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    cmake_minimum_required(VERSION 2.8)
    project(libbladeRF_test_peripheral_timing C)

    set(INCLUDES
            ${libbladeRF_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
            ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    )

    set(SRC
        main.c
        ../common/src/test_common.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    )

    if(LIBC_VERSION)
        # clock_gettime() was moved from librt -> libc in 2.17
        if(${LIBC_VERSION} VERSION_LESS "2.17")
            set(CLI_LINK_LIBRARIES ${CLI_LINK_LIBRARIES} rt)
        endif()
    endif()

    include_directories(${INCLUDES})
    add_executable(libbladeRF_test_peripheral_timing ${SRC})
    target_link_libraries(libbladeRF_test_peripheral_timing libbladerf_shared)
endif()
