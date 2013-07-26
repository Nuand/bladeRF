SRC := $(wildcard *.c) $(wildcard cmd/*.c)
OBJ = $(SRC:.c=.o)

BIN_DIR ?= .
CFLAGS_ := $(CFLAGS) -Icmd -Iinteractive -I. -pthread
LDFLAGS_ := $(LDFLAGS) -pthread

ifneq ($(INTERACTIVE),n)
	CFLAGS_ += -DINTERACTIVE
	LDFLAGS_ += -ltecla
	SRC += $(wildcard interactive/*.c)
endif

TARGET = $(BIN_DIR)/bladeRF-cli

all: $(TARGET)

%.o : %.c
	$(CC) $< $(CFLAGS_) -c -o $@

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS_) -o $(TARGET)

clean:
	rm -f bladeRF
	rm -f $(OBJ)

.PHONY: clean
