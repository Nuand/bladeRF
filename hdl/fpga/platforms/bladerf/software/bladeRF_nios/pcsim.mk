#------------------------------------------------------------------------------
#              VARIABLES TO BE DEFINED BY PLATFORM MAKEFILE
#------------------------------------------------------------------------------

FPGA_COMMON_DIR    := ../../../../../../fpga_common
BLADERF_COMMON_DIR := ../../../common/bladerf/software/bladeRF_nios

QUARTUS_WORKDIR    := ../../../../../quartus/work/bladerf
NIOS_BUILD_OUTDIR  := $(QUARTUS_WORKDIR)/bladeRF_nios

#------------------------------------------------------------------------------
#              INCLUDE THE COMMON MAKEFILE
#------------------------------------------------------------------------------

include $(BLADERF_COMMON_DIR)/pcsim.mk
