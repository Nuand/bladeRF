#------------------------------------------------------------------------------
#              VARIABLES TO BE DEFINED BY PLATFORM MAKEFILE
#------------------------------------------------------------------------------

#  - FPGA_COMMON_DIR    : Path to fpga_common
#  - BLADERF_COMMON_DIR : Path to common bladeRF_nios dir
#  - NIOS_BUILD_OUTDIR  : Path to place the Nios build products

#------------------------------------------------------------------------------
#              MAKEFILE TARGETS
#------------------------------------------------------------------------------

INCLUDES := -I $(FPGA_COMMON_DIR)/include -I $(BLADERF_COMMON_DIR)/src

CFLAGS := -Wall -Wextra -Wno-unused-parameter \
          -O0 -ggdb3 -DBLADERF_NIOS_PC_SIMULATION \
          $(INCLUDES)

SRC := $(wildcard $(BLADERF_COMMON_DIR)/src/*.c) $(FPGA_COMMON_DIR)/src/band_select.c

all: $(NIOS_BUILD_OUTDIR)/bladeRF_nios.sim

bladeRF_nios.sim: $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(NIOS_BUILD_OUTDIR)/bladeRF_nios.sim

.PHONY: clean
