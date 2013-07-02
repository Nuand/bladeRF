# bladeRF FX3 Firmware
#
# Be sure to configure you toolchain paths in make/toolchain.mk
################################################################################

ifeq ($(wildcard make/toolchain.mk),)
$(error Copy make/toolchain.mk.sample to make/toolchain.mk and edit paths accordingly before running make)
endif
include make/toolchain.mk

# Target files
TARGET := bladeRF
ELF := $(TARGET).elf
IMG := $(TARGET).img
MAP := $(TARGET).map

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o) $(TOOLCHAIN_DEPS_OBJ) 

CFLAGS := -Wall -Wextra -Wno-unused-parameter \
		  $(TOOLCHAIN_CFLAGS) \
		  $(EXTRA_CFLAGS)

ifdef DEBUG
	CFLAGS += -O0 -ggdb3 -DDEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

LDFLAGS := $(TOOLCHAIN_LDFLAGS) -Map $(MAP)

all: $(IMG)

$(ELF): $(OBJ) 
	$(LD) $(OBJ) $(LDFLAGS) -o $@

$(IMG): $(ELF) $(ELF2IMG)
	$(ELF2IMG) -i $< -o $@

$(ELF2IMG): $(ELF2IMG_SRC)
	$(HOST_CC) $< -O2 -o $@

# Build any toolchain dependencies. 
$(TOOLCHAIN_DEPS_OBJ_C): %.o : $(TOOLCHAIN_DEPS_DIR)/%.c
	$(call build_toolchain_dep,$<,$@)

$(TOOLCHAIN_DEPS_OBJ_A): %.o : $(TOOLCHAIN_DEPS_DIR)/%.S
	$(call build_toolchain_dep,$<,$@)

clean:
	$(RM) -f $(ELF) $(IMG) $(MAP) $(OBJ) $(TOOLCHAIN_OBJ)

realclean: clean
	$(RM) -f elf2img

.PHONY: clean realclean
