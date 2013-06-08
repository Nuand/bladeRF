SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

BIN_DIR ?= .

FLAGS ?= $(CFLAGS)

ifdef INTERACTIVE
FLAGS += -ltecla -DINTERACTIVE
endif

TARGET = $(BIN_DIR)/bladeRF

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS) $(FLAGS) -o $(TARGET)

clean:
	rm -f bladeRF
	rm -f $(OBJ)

.PHONY: clean
