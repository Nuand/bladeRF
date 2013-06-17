SRC := $(wildcard *.c) $(wildcard cmd/*.c)
OBJ = $(SRC:.c=.o)

BIN_DIR ?= .
CFLAGS_ := $(CFLAGS) -Icmd -Iinteractive -I.
LDFLAGS_ := $(LDFLAGS)

ifneq ($(INTERACTIVE),n)
	CFLAGS_ += -DINTERACTIVE
	LDFLAGS_ += -ltecla
	SRC += $(wildcard interactive/*.c)
endif

TARGET = $(BIN_DIR)/bladeRF

all: $(TARGET)

%.o : %.c
	$(CC) $< $(CFLAGS_) -c -o $@

$(TARGET): $(OBJ)
	$(CC) $^ $(LDFLAGS_) -o $(TARGET)

clean:
	rm -f bladeRF
	rm -f $(OBJ)

.PHONY: clean
