################################################################################
# libbladeRF
#
# Quick Makefile, until we get autotools in order
################################################################################

# XXX Quickly enabling these items for the time being
ENABLE_BACKEND_LIBUSB=y
ENABLE_BACKEND_LINUX_DRIVER=y

# Fetch the library version
include make/version.mk

LIB_NAME := libbladeRF

BIN_DIR := bin
SRC_DIR := src
INC_DIR := include
LIB_DIR := lib
BIN_DIR := bin

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)

# Craft the various file names associated with the library
LIB_SO_NAME := $(LIB_NAME).so.$(LIB_VER_MAJ)
LIB_NAME_FULL := $(LIB_SO_NAME).$(LIB_VER_MIN).$(LIB_VER_PAT)

SHARED_LIB := $(LIB_DIR)/$(LIB_NAME_FULL)
STATIC_LIB := $(LIB_DIR)/$(LIB_NAME).a

# Makes LIB_VERSION macro definition available to code
LIB_VER_FLAG := -DLIB_VERSION=\"v$(LIB_VER_MAJ).$(LIB_VER_MIN).$(LIB_VER_PAT)\"

# Path to driver interface headers
# Changing default to make our lives easier for now...
##DRIVER_HEADER_DIR ?= /usr/include
DRIVER_HEADER_DIR ?= ../../common


CFLAGS := -Wall -Wextra -Wno-unused-parameter \
		  -fPIC \
		  -std=gnu99 -D_GNU_SOURCE $(LIB_VER_FLAG) \
		  -I$(SRC_DIR) -I$(SRC_DIR)/backends \
		  -I$(INC_DIR) -I$(DRIVER_HEADER_DIR)

LDFLAGS := -fPIC

# Set DEBUG to enable a debug build, otherwise default to a release build
ifdef DEBUG
	CFLAGS += -O0 -ggdb3 -DDEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

# Enable libusb (>= 1.0) support by defining ENABLE_BACKEND_LIBUSB
ifdef ENABLE_BACKEND_LIBUSB
	SRC += $(SRC_DIR)/backends/libusb.c
	CFLAGS += -DENABLE_BACKEND_LIBUSB
	LDFLAGS += -lusb-1.0
endif

# Enable Linux kernel driver backend by defining ENABLE_BACKEND_LINUX_DRIVER
ifdef ENABLE_BACKEND_LINUX_DRIVER
	SRC += $(SRC_DIR)/backends/driver_linux.c
	CFLAGS += -DENABLE_BACKEND_LINUX_DRIVER
endif

# Default install prefix
INSTALL_PREFIX ?= /usr

all: shared static

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

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
		ln -fs $(LIB_SO_NAME) $(LIB_NAME).so

libbladeRF.pc: shared
	@echo "prefix=${INSTALL_PREFIX}" > $@
	@echo "exec_prefix=$$""{prefix}" >> $@
	@echo "libdir=$$""{exec_prefix}/lib" >> $@
	@echo "includedir=$$""{exec_prefix}/include" >> $@
	@echo -e "\nName: libbladeRF" >> $@
	@echo "Description: C library for nuand bladeRF devices" >> $@
	@echo "URL: http://www.nuand.com" >> $@
	@echo "Version: ${LIB_VER}" >> $@
	@echo "Libs: -L$$""{libdir} -lbladeRF" >> $@
	@echo "Cflags: -I$$""{includedir}"  >> $@

doc:
	$(MAKE) -C doc/doxygen

install: shared libbladeRF.pc
	cp -P lib/* $(INSTALL_PREFIX)/lib
	cp include/* $(INSTALL_PREFIX)/include
	if [ -d $(INSTALL_PREFIX)/share/pkgconfig ]; then \
		cp libbladeRF.pc $(INSTALL_PREFIX)/share/pkgconfig/ ; \
	elif [ -d $(INSTALL_PREFIX)/lib/pkgconfig ]; then \
		cp libbladeRF.pc $(INSTALL_PREFIX)/lib/pkgconfig/ ; \
	else \
		@echo "Unsure where to install libbladeRF.pc ... aborting." ; \
		exit 1; \
	fi

uninstall:
	rm -f $(INSTALL_PREFIX)/lib/libbladeRF*
	rm -f $(INSTALL_PREFIX)/include/libbladeRF.h
	rm -f $(INSTALL_PREFIX)/lib/pkgconfig/libbladeRF.pc
	rm -f $(INSTALL_PREFIX)/share/pkgconfig/libbladeRF.pc

clean:
	rm -rf $(LIB_DIR)
	rm -rf $(BIN_DIR)
	rm -f $(OBJ)
	rm -f libbladeRF.pc
	$(MAKE) -C doc/doxygen clean

.PHONY: clean doc
