# bladeRF FX3 Firmware
#
# Be sure to configure you toolchain paths in make/toolchain.mk
################################################################################

ifeq ($(wildcard make/toolchain.mk),)
$(error Copy make/toolchain.mk.sample to make/toolchain.mk and edit paths accordingly before running make)
endif
include make/toolchain.mk

TARGET := bladeRF

# bladeRF code
BLADERF_SOURCE_C := $(wildcard *.c)
BLADERF_SOURCE_ASM := $(wildcard *.S)
BLADERF_OBJ_C := $(BLADERF_SOURCE_C:.c=.o)
BLADERF_OBJ_ASM := $(BLAERF_SOURCE__ASM:.S=.o)

# Items provided in the toolchain
TOOLCHAIN_OBJ_C := $(notdir $(SOURCE:.c=.o))
TOOLCHAIN_OBJ_ASM := $(notdir $(SOURCE_ASM:.S=.o))

OBJ := $(BLADERF_OBJ_C) $(BLADERF_OBJ_ASM) \
		$(TOOLCHAIN_OBJ_C) $(TOOLCHAIN_OBJ_ASM)

# Target files
ELF := $(TARGET).$(EXEEXT)
IMG := $(TARGET).img
MAP := $(TARGET).map

all: $(IMG)

$(ELF): $(OBJ)
	$(LINK)

$(IMG): $(ELF) elf2img
	./elf2img -i $< -o $@

elf2img: $(ELF2IMG_PATH)/elf2img.c
	$(HOST_CC) -Wall -O2 $^ -o $@

$(BLADERF_OBJ_C): %.o : %.c
	$(COMPILE)

$(TOOLCHAIN_OBJ_C): %.o : $(FX3FWROOT)/common/%.c
	$(COMPILE)

$(BLADERF_OBJ_A): %.o : %.S
	$(ASSEMBLE)

$(TOOLCHAIN_OBJ_ASM): %.o : $(FX3FWROOT)/common/%.S
	$(ASSEMBLE)

clean:
	rm -f $(ELF) $(IMG) $(MAP) $(OBJ)

realclean: clean
	rm -f elf2img


.PHONY: clean
