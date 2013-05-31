SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

CFLAGS := -Wall -Wextra -Wno-unused-parameter -ggdb3 -O0 -I../../include
LDFLAGS := -lbladerf -L ../../lib

all: bladerf

bladerf: $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f bladerf
	rm -f $(OBJ)

.PHONY: clean
