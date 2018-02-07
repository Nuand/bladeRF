cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_config_file C)

set(TEST_CONFIG_FILE_INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_FPGA_COMMON_INCLUDE_DIR}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

set(SRC
    src/main.c
    ../common/src/test_common.c
    ${libbladeRF_SOURCE_DIR}/src/bladerf_priv.c
    ${libbladeRF_SOURCE_DIR}/src/config.c
    ${libbladeRF_SOURCE_DIR}/src/dc_cal_table.c
    ${libbladeRF_SOURCE_DIR}/src/file_ops.c
    ${libbladeRF_SOURCE_DIR}/src/flash.c
    ${libbladeRF_SOURCE_DIR}/src/flash_fields.c
    ${libbladeRF_SOURCE_DIR}/src/fpga.c
    ${libbladeRF_SOURCE_DIR}/src/si5338.c
    ${libbladeRF_SOURCE_DIR}/src/tuning.c
    ${libbladeRF_SOURCE_DIR}/src/version_compat.c
    ${libbladeRF_SOURCE_DIR}/src/xb.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/parse.c
    ${BLADERF_FPGA_COMMON_SOURCE_DIR}/band_select.c
    ${BLADERF_FPGA_COMMON_SOURCE_DIR}/lms.c
)

set(TEST_CONFIG_FILE_LIBS libbladerf_shared)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    set(TEST_CONFIG_FILE_INCLUDES
        ${TEST_CONFIG_FILE_INCLUDES}
        ${MSVC_C99_INCLUDES}
        ${LIBPTHREADSWIN32_INCLUDE_DIRS}
    )
    set(SRC
        ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/mkdtemp.c
    )
    set(TEST_CONFIG_FILE_LIBS
        ${TEST_CONFIG_FILE_LIBS}
        ${LIBPTHREADSWIN32_LIBRARIES}
    )
endif()

add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")
add_definitions(-DLOGGING_ENABLED=1)

include_directories(${TEST_CONFIG_FILE_INCLUDES})
add_executable(libbladeRF_test_config_file ${SRC})
target_link_libraries(libbladeRF_test_config_file ${TEST_CONFIG_FILE_LIBS})
