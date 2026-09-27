cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_flash_fpga_compression C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma
)

set(SRC
    src/main.c
    ${libbladeRF_SOURCE_DIR}/src/board/bladerf1/flash.c
    ${libbladeRF_SOURCE_DIR}/src/driver/spi_flash.c
    ${libbladeRF_SOURCE_DIR}/src/helpers/version.c
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma/CpuArch.c
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma/LzFind.c
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma/LzFindOpt.c
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma/LzmaDec.c
    ${libbladeRF_SOURCE_DIR}/../../../thirdparty/lzma/LzmaEnc.c
)

include_directories(${INCLUDES})
add_executable(${PROJECT_NAME} ${SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE Z7_ST)
