################################################################################
# libbladerf
#
# Quick Makefile, until we get autotools in order
################################################################################

# Fetch the library version
include make/version.mk

LIB_NAME := libbladerf

SRC_DIR := src
INC_DIR := include
LIB_DIR := lib
BIN_DIR := bin

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:.c=.o)

# Craft the various file names associated with the library
LIB_SO_NAME := $(LIB_NAME).so.$(LIB_VER_MAJ)
LIB_NAME_FULL := $(LIB_SO_NAME).$(LIB_VER_MIN).$(LIB_VER_PAT)

SHARED_LIB := $(LIB_DIR)/$(LIB_NAME_FULL)
STATIC_LIB := $(LIB_DIR)/$(LIB_NAME).a

# Makes LIB_VERSION macro definition available to code
LIB_VER_FLAG := -DLIB_VERSION=\"v$(LIB_VER_MAJ).$(LIB_VER_MIN).$(LIB_VER_PAT)\"

# Path to driver interface headers
DRIVER_HEADER_DIR ?= /usr/include

CFLAGS := -Wall -Wextra -Wno-unused-parameter -fPIC -std=gnu99 $(LIB_VER_FLAG) \
			-I$(INC_DIR) -I$(DRIVER_HEADER_DIR)
LDFLAGS :=

ifdef DEBUG
	CFLAGS += -O0 -ggdb3
else
	CFLAGS += -O2
endif

# Default install prefix
INSTALL_PREFIX ?= /usr

all: shared static

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Build static library
static: $(LIB_DIR) $(STATIC_LIB)

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $@ $(OBJ)

# Build shared library and all the version symlinks
shared: $(LIB_DIR) $(SHARED_LIB)

$(SHARED_LIB): $(OBJ)
	$(CC) -shared -Wl,-soname,$(LIB_SO_NAME) $(OBJ) $(LDFLAGS) -o $@
	@cd $(LIB_DIR); \
		ln -fs $(LIB_NAME_FULL) $(LIB_SO_NAME).$(LIB_VER_MIN); \
		ln -fs $(LIB_SO_NAME).$(LIB_VER_MIN) $(LIB_SO_NAME); \

doc:
	$(MAKE) -C doc/doxygen

install: shared
	cp lib/* $(INSTALL_PREFIX)/lib
	cp include/* $(INSTALL_PREFIX)/include

uninstall:
	rm -f $(INSTALL_PREFIX)/lib/libbladerf*
	rm -f $(INSTALL_PREFIX)/include/bladef.h

clean:
	rm -rf $(LIB_DIR)
	rm -f $(OBJ)
	$(MAKE) -C doc/doxygen clean

.PHONY: clean doc
