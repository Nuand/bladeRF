################################################################################
# bladeRF 2.0 micro Nios firmware for Nios V
#
# hdl/quartus/build_bladerf.sh configures this project in the Quartus work
# directory after generating the HAL BSP with niosv-bsp:
#
#   cmake -S <this dir> -B <work dir>/bladeRF_nios \
#         -DBSP_DIR=<work dir>/bladeRF_nios_bsp -DCMAKE_BUILD_TYPE=Release
#   cmake --build <work dir>/bladeRF_nios
#
# It produces bladeRF_nios.elf and mem_init/nios_system_ram.hex, the on-chip
# RAM image that the Quartus project picks up through mem_init/meminit.qip.
#
# The Nios II firmware is built by the Makefile in this directory instead.
# Keep the source list and flags below in sync with it; the part shared with
# the other Nios V applications (BSP, C library, common sources and flags, RAM
# image) is in ../../../common/bladerf/software/bladeRF_nios/niosv_app.cmake.
################################################################################
cmake_minimum_required(VERSION 3.14)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/bladerf/software/bladeRF_nios/niosv_app.cmake)

project(bladeRF_nios C ASM)

# AD936x driver (ADI no-OS, patched), built with the same toolchain and flags
add_subdirectory(${BLADERF_ROOT}/hdl/fpga/ip/analogdevicesinc/no_OS libad936x)

bladerf_niosv_firmware(bladeRF_nios
    SOURCES
        # bladeRF 2.0 micro sources (Makefile in this directory)
        src/bladeRF_nios.c
        ${BLADERF_ROOT}/fpga_common/src/ad936x_helpers.c
        ${BLADERF_ROOT}/fpga_common/src/bladerf2_common.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic_cmds.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic_queue.c
        ${BLADERF_NIOS_COMMON_DIR}/src/pkt_retune2.c
        ${BLADERF_ROOT}/host/common/src/range.c
    INCLUDE_DIRECTORIES
        src
        ${BLADERF_ROOT}/host/common/include
        ${BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS_local/platform_bladerf2
    COMPILE_DEFINITIONS
        BOARD_BLADERF_MICRO
    LIBRARIES
        ad936x
)
