#------------------------------------------------------------------------------
#              VARIABLES TO BE DEFINED BY PLATFORM MAKEFILE
#------------------------------------------------------------------------------

FPGA_COMMON_DIR    := ../../../../../../fpga_common
LIBBLADERF_DIR     := ../../../../../../host/libraries/libbladeRF
BLADERF_COMMON_DIR := ../../../common/bladerf/software/bladeRF_nios

QUARTUS_WORKDIR    := ../../../../../quartus/work/bladerf
NIOS_BUILD_OUTDIR  := $(QUARTUS_WORKDIR)/bladeRF_nios
MEM_INIT_DIR       := $(NIOS_BUILD_OUTDIR)/mem_init
BSP_BUILD_OUTDIR   := $(QUARTUS_WORKDIR)/bladeRF_nios_bsp
QUARTUS_OUTDIR     := $(QUARTUS_WORKDIR)/output_files

ALT_INCLUDE_DIRS   += -I ./src

C_SRCS             += ./src/bladeRF_nios.c

#------------------------------------------------------------------------------
#              INCLUDE THE COMMON MAKEFILE
#------------------------------------------------------------------------------

include $(BLADERF_COMMON_DIR)/Makefile
