CFLAGS := -Wall -Wextra -Wno-unused-parameter \
          -O0 -ggdb3 -DBLADERF_NIOS_PC_SIMULATION

SRC := $(wildcard src/*.c)

all: bladeRF_nios.sim

bladeRF_nios.sim: $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f bladeRF_nios.sim

.PHONY: clean
