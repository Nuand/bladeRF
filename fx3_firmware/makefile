# bladeRF FX3 Firmware
#
# Be sure to configure you toolchain paths in make/toolchain.mk
################################################################################

ifeq ($(wildcard make/toolchain.mk),)
$(error Copy make/toolchain.mk.sample to make/toolchain.mk and edit paths accordingly before running make)
endif
include make/toolchain.mk

# Target files
OUTPUT_DIR = build
TARGET := bladeRF
ELF := $(OUTPUT_DIR)/$(TARGET).elf
IMG := $(OUTPUT_DIR)/$(TARGET).img
MAP := $(OUTPUT_DIR)/$(TARGET).map


SRC := $(wildcard *.c)
OBJ := $(addprefix $(OUTPUT_DIR)/,$(SRC:.c=.o)) \
	$(addprefix $(OUTPUT_DIR)/,$(TOOLCHAIN_DEPS_OBJ))

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

$(OUTPUT_DIR)/.dirstamp:
	mkdir -p $(OUTPUT_DIR)
	touch $@

$(OUTPUT_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

$(ELF): $(OUTPUT_DIR)/.dirstamp $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

$(IMG): $(ELF) $(OUTPUT_DIR)/$(ELF2IMG)
	$(OUTPUT_DIR)/$(ELF2IMG) -i $< -o $@

$(OUTPUT_DIR)/$(ELF2IMG): $(ELF2IMG_SRC)
	$(HOST_CC) $< -O2 -o $@

# Build any toolchain dependencies. 
$(OUTPUT_DIR)/$(TOOLCHAIN_DEPS_OBJ_C): $(OUTPUT_DIR)/%.o : $(TOOLCHAIN_DEPS_DIR)/%.c
	$(call build_toolchain_dep,$<,$@)

$(OUTPUT_DIR)/$(TOOLCHAIN_DEPS_OBJ_A): $(OUTPUT_DIR)/%.o : $(TOOLCHAIN_DEPS_DIR)/%.S
	$(call build_toolchain_dep,$<,$@)

clean:
	$(RM) -f $(ELF) $(IMG) $(MAP) $(OBJ) $(TOOLCHAIN_OBJ)

realclean: clean
	$(RM) -f $(OUTPUT_DIR)/$(ELF2IMG)
	$(RM) -rf $(OUTPUT_DIR)

.PHONY: clean realclean
