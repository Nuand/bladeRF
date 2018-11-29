#------------------------------------------------------------------------------
#              VARIABLES TO BE DEFINED BY PLATFORM MAKEFILE
#------------------------------------------------------------------------------

FPGA_COMMON_DIR     := ../../../../../../fpga_common
LIBBLADERF_DIR      := ../../../../../../host/libraries/libbladeRF
BLADERF_COMMON_DIR  := ../../../common/bladerf/software/bladeRF_nios
HOST_COMMON_DIR     := ../../../../../../host/common
FIRMWARE_COMMON_DIR := ../../../../../../firmware_common

QUARTUS_WORKDIR     := ../../../../../quartus/${WORKDIR}
NIOS_BUILD_OUTDIR   := $(QUARTUS_WORKDIR)/bladeRF_nios
MEM_INIT_DIR        := $(NIOS_BUILD_OUTDIR)/mem_init
BSP_BUILD_OUTDIR    := $(QUARTUS_WORKDIR)/bladeRF_nios_bsp
QUARTUS_OUTDIR      := $(QUARTUS_WORKDIR)/output_files

ALT_INCLUDE_DIRS    += ./src

C_SRCS              += ./src/bladeRF_nios.c
C_SRCS              += $(FPGA_COMMON_DIR)/src/lms.c
C_SRCS              += $(FPGA_COMMON_DIR)/src/band_select.c
C_SRCS              += $(BLADERF_COMMON_DIR)/src/pkt_retune.c

CFLAGS              += -DBOARD_BLADERF

#------------------------------------------------------------------------------
#              INCLUDE THE COMMON MAKEFILE
#------------------------------------------------------------------------------

include $(BLADERF_COMMON_DIR)/Makefile
