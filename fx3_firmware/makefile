# bladeRF FX3 Firmware
#
# Be sure to configure you toolchain paths in make/toolchain.mk
################################################################################

ifeq ($(wildcard make/toolchain.mk),)
$(error Copy make/toolchain.mk.sample to make/toolchain.mk and edit paths accordingly before running make)
endif
OUTPUT_DIR = build
include make/toolchain.mk

# Target files
TARGET := bladeRF
ELF := $(OUTPUT_DIR)/$(TARGET).elf
IMG := $(OUTPUT_DIR)/$(TARGET).img
MAP := $(OUTPUT_DIR)/$(TARGET).map


SRC := $(wildcard *.c)
OBJ := $(addprefix $(OUTPUT_DIR)/,$(SRC:.c=.o)) \
	$(addprefix $(OUTPUT_DIR)/,$(TOOLCHAIN_DEPS_OBJ))

CFLAGS := -Wall -Wextra -Wno-unused-parameter \
		  $(TOOLCHAIN_CFLAGS) \
		  $(EXTRA_CFLAGS) \
		  -I$(OUTPUT_DIR)

ifdef DEBUG
	CFLAGS += -O0 -ggdb3 -DDEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

LDFLAGS := $(TOOLCHAIN_LDFLAGS) -Map $(MAP)


all: $(OUTPUT_DIR)/version.h $(IMG)

$(OUTPUT_DIR)/.dirstamp:
	-@mkdir -p $(OUTPUT_DIR)
	@echo THERE_IS_NO_TOUCH_ON_WINDOWS_DO_NOT_ALTER_THIS_LINE > $(OUTPUT_DIR)/.dirstamp

$(OUTPUT_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

$(ELF): $(OUTPUT_DIR)/.dirstamp $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

$(IMG): $(ELF) $(ELF2IMG)
	$(ELF2IMG) -i $< -o $@

$(ELF2IMG): $(ELF2IMG_SRC)
	$(HOST_CC) $< -O2 -o $@

# Build any toolchain dependencies. 
$(OUTPUT_DIR)/cyfxtx.o: $(OUTPUT_DIR)/%.o : $(TOOLCHAIN_DEPS_DIR)/%.c
	$(call build_toolchain_dep,$<,$@)

$(OUTPUT_DIR)/$(TOOLCHAIN_DEPS_OBJ_A): $(OUTPUT_DIR)/%.o : $(TOOLCHAIN_DEPS_DIR)/%.S
	$(call build_toolchain_dep,$<,$@)

$(OUTPUT_DIR)/version.h: $(OUTPUT_DIR)/.dirstamp version.h.in
	cd $(OUTPUT_DIR) && \
		cmake $(CURDIR)
	-@cp $(OUTPUT_DIR)/build/version.h $(OUTPUT_DIR)/version.h

clean:
	$(RM) -rf $(OUTPUT_DIR)

realclean: clean

.PHONY: clean realclean
