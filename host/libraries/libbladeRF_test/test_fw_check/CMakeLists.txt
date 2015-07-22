cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_fw_check C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(SRC
    ${libbladeRF_SOURCE_DIR}/src/fx3_fw.c
    ${libbladeRF_SOURCE_DIR}/src/file_ops.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

set(SRC_TO_SHORTEN ${SRC})
include(ShortFileMacro)

add_definitions(-DLOGGING_ENABLED)
add_definitions(-DTEST_FX3_FW_VALIDATION)
add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")

include_directories(${INCLUDES})
add_executable(libbladeRF_test_fw_check ${SRC})
