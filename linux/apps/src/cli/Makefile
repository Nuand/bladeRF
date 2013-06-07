SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

BIN_DIR ?= .

TARGET = $(BIN_DIR)/bladeRF

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $(TARGET)

clean:
	rm -f bladeRF
	rm -f $(OBJ)

.PHONY: clean
