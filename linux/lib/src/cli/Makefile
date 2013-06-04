SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

CFLAGS := -Wall -Wextra -Wno-unused-parameter -ggdb3 -O0 -I../../include
LDFLAGS := -lbladeRF -L ../../lib

all: bladeRF

bladeRF: $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f bladeRF
	rm -f $(OBJ)

.PHONY: clean
