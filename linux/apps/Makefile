################################################################################
# bladeRF applications and utilities
#
# Temporary Makefile until we get an autotools-based build in place
#
# If you don't have libbladeRF installed on your system provide
# CFLAGS_LIBBLADERF and LDFLAGS_LIBBLADERF at build-time. Be aware that
# the build dives down a couple dirs, so either adjust your relative
# paths accordingly, or use absolute paths (preferred)
################################################################################

SRC_DIR := src
BIN_DIR := bin

APPS := cli

CFLAGS_LIBBLADERF ?= $(shell pkg-config --cflags libbladeRF)
CFLAGS := -Wall -Wextra -Wno-unused-parameter -std=gnu99 -D_GNU_SOURCE \
			$(CFLAGS_LIBBLADERF)

LDFLAGS_LIBBLADERF ?= $(shell pkg-config --libs libbladeRF)
LDFLAGS := $(LDFLAGS_LIBBLADERF)

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
		BIN_DIR="$(abspath $(dir $@))" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" INTERACTIVE="$(INTERACTIVE)"

clean:
	rm -rf $(BIN_DIR)
	$(MAKE) -C $(SRC_DIR)/cli clean

