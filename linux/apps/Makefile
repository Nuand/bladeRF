################################################################################
# bladeRF applications and utilities
#
# Temporary Makefile until we get an autotools-based build in place
################################################################################

SRC_DIR := src
BIN_DIR := bin

APPS := cli

CFLAGS ?= -Wall -Wextra -Wno-unused-parameter -std=gnu99 -D_GNU_SOURCE \
		  $(shell pkg-config --cflags libbladeRF)

LDFLAGS ?= $(shell pkg-config --libs libbladeRF)

ifdef DEBUG
	CFLAGS += -O0 -ggdb3 -DDEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

all: $(APPS)

$(APPS): % : $(BIN_DIR)/% $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/% : $(BIN_DIR)
	$(MAKE) -C $(SRC_DIR)/$(notdir $@) \
		BIN_DIR="$(abspath $(dir $@))" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

clean:
	rm -rf $(BIN_DIR)
	$(MAKE) -C $(SRC_DIR)/cli clean

