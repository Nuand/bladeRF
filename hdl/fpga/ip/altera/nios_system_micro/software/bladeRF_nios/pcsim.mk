FPGA_COMMON_DIR := ../../../../../../../fpga_common
INCLUDES := -I $(FPGA_COMMON_DIR)/include -I src

CFLAGS := -Wall -Wextra -Wno-unused-parameter \
          -O0 -ggdb3 -DBLADERF_NIOS_PC_SIMULATION \
          $(INCLUDES)

SRC := $(wildcard src/*.c) $(FPGA_COMMON_DIR)/src/band_select.c

all: bladeRF_nios.sim

bladeRF_nios.sim: $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f bladeRF_nios.sim

.PHONY: clean
