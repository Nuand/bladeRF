################################################################################
# Foxhunt firmware for the bladeRF 2.0 micro's Nios V (foxhunt revision)
#
# hdl/quartus/build_bladerf.sh builds it after bladeRF_nios when building the
# foxhunt revision with Nios V:
#
#   cmake -S <this dir> -B <work dir>/foxhunt \
#         -DBSP_DIR=<work dir>/bladeRF_nios_bsp -DCMAKE_BUILD_TYPE=Release
#   cmake --build <work dir>/foxhunt
#
# Its RAM image, <work dir>/foxhunt/mem_init/nios_system_ram.hex, is the one
# the foxhunt revision uses (../../bladerf-foxhunt.qip).
#
# The Nios II firmware is built by the Makefile in this directory instead.
# Keep the source list and flags below in sync with it; the part shared with
# the other Nios V applications is in
# ../../../common/bladerf/software/bladeRF_nios/niosv_app.cmake.
################################################################################
cmake_minimum_required(VERSION 3.14)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../common/bladerf/software/bladeRF_nios/niosv_app.cmake)

project(foxhunt C ASM)

# AD936x driver (ADI no-OS, patched), built with the same toolchain and flags
add_subdirectory(${BLADERF_ROOT}/hdl/fpga/ip/analogdevicesinc/no_OS libad936x)

bladerf_niosv_firmware(foxhunt
    SOURCES
        # Foxhunt sources (Makefile in this directory)
        src/main.c
        src/morse.c
        ${BLADERF_ROOT}/fpga_common/src/ad936x_helpers.c
        ${BLADERF_ROOT}/fpga_common/src/bladerf2_common.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic_cmds.c
        ${BLADERF_NIOS_COMMON_DIR}/src/devices_rfic_queue.c
        ${BLADERF_ROOT}/host/common/src/range.c
    INCLUDE_DIRECTORIES
        src
        ../bladeRF_nios/src
        ${BLADERF_ROOT}/host/common/include
        ${BLADERF_ROOT}/thirdparty/analogdevicesinc/no-OS_local/platform_bladerf2
    COMPILE_DEFINITIONS
        BOARD_BLADERF_MICRO
    LIBRARIES
        ad936x
)
