
#########################################################################
#######     M E M   I N I T    M A K E F I L E   C O N T E N T     ######
#########################################################################

#########################################################################
# This file is intended to be included by public.mk
#
#
# The following variables must be defined before including this file:
# - ELF
#
# The following variables may be defined to override the default behavior:
# - HDL_SIM_DIR
# - HDL_SIM_INSTALL_DIR
# - MEM_INIT_DIR
# - MEM_INIT_INSTALL_DIR
# - QUARTUS_PROJECT_DIR
# - SOPC_NAME
# - SIM_OPTIMIZE
# - RESET_ADDRESS
#
#########################################################################

ifeq ($(MEM_INIT_FILE),)
# MEM_INIT_FILE should be set equal to the working relative path to this
# mem_init.mk makefile fragment
MEM_INIT_FILE := $(wildcard $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
endif

ifeq ($(ELF2DAT),)
ELF2DAT := elf2dat
endif

ifeq ($(ELF2HEX),)
ELF2HEX := elf2hex
endif

ifeq ($(ELF2FLASH),)
ELF2FLASH := elf2flash
endif

ifeq ($(FLASH2DAT),)
FLASH2DAT := flash2dat
endif

ifeq ($(NM),)
NM := nios2-elf-nm
endif

ifeq ($(MKDIR),)
MKDIR := mkdir -p
endif

ifeq ($(RM),)
RM := rm -f
endif

ifeq ($(CP),)
CP := cp
endif

ifeq ($(ECHO),)
ECHO := echo
endif

MEM_INIT_DIR ?= mem_init
HDL_SIM_DIR ?= $(MEM_INIT_DIR)/hdl_sim

ifdef QUARTUS_PROJECT_DIR
MEM_INIT_INSTALL_DIR ?= $(patsubst %/,%,$(QUARTUS_PROJECT_DIR))
ifdef SOPC_NAME
HDL_SIM_INSTALL_DIR ?= $(patsubst %/,%,$(QUARTUS_PROJECT_DIR))/$(SOPC_NAME)_sim
endif
endif

MEM_INIT_DESCRIPTOR_FILE ?= $(MEM_INIT_DIR)/meminit.spd

MEM_INIT_QIP_FILE ?= $(MEM_INIT_DIR)/meminit.qip

#-------------------------------------
# Default Flash Boot Loaders
#-------------------------------------

BOOT_LOADER_PATH ?= $(SOPC_KIT_NIOS2)/components/altera_nios2
BOOT_LOADER_CFI ?= $(BOOT_LOADER_PATH)/boot_loader_cfi.srec
BOOT_LOADER_CFI_BE ?= $(BOOT_LOADER_PATH)/boot_loader_cfi_be.srec


#-------------------------------------
# Default Target
#-------------------------------------

.PHONY: default_mem_init
ifeq ($(QSYS),1)
default_mem_init: mem_init_generate
else
default_mem_init: mem_init_install
endif
#-------------------------------------
# Runtime Macros
#-------------------------------------

define post-process-info
	@echo Post-processing to create $@...
endef

target_stem = $(notdir $(basename $@))

mem_start_address = $($(target_stem)_START)
mem_end_address = $($(target_stem)_END)
mem_width = $($(target_stem)_WIDTH)
mem_endianness = $($(target_stem)_ENDIANNESS)
mem_create_lanes = $($(target_stem)_CREATE_LANES)

mem_pad_flag = $($(target_stem)_PAD_FLAG)
mem_reloc_input_flag = $($(target_stem)_RELOC_INPUT_FLAG)
mem_no_zero_fill_flag = $($(target_stem)_NO_ZERO_FILL_FLAG)

flash_mem_epcs_flag = $($(target_stem)_EPCS_FLAGS)
flash_mem_cfi_flag = $($(target_stem)_CFI_FLAGS)
flash_mem_boot_loader_flag = $($(target_stem)_BOOT_LOADER_FLAG)

elf2dat_extra_args = $(mem_pad_flag)
elf2hex_extra_args = $(mem_no_zero_fill_flag)
elf2flash_extra_args = $(flash_mem_cfi_flag) $(flash_mem_epcs_flag) $(flash_mem_boot_loader_flag)
flash2dat_extra_args = $(mem_pad_flag) $(mem_reloc_input_flag)

#------------------------------------------------------------------------------
#                              BSP SPECIFIC CONTENT
#
# The content below is controlled by the BSP and SOPC System
#------------------------------------------------------------------------------
#START OF BSP SPECIFIC

#-------------------------------------
# Global Settings
#-------------------------------------


# The following TYPE comment allows tools to identify the 'type' of target this 
# makefile is associated with. 
# TYPE: BSP_MEMINIT_MAKEFILE

# This following VERSION comment indicates the version of the tool used to 
# generate this makefile. A makefile variable is provided for VERSION as well. 
# ACDS_VERSION: 12.1sp1
ACDS_VERSION := 12.1sp1

# This following BUILD_NUMBER comment indicates the build number of the tool 
# used to generate this makefile. 
# BUILD_NUMBER: 243

# Optimize for simulation
SIM_OPTIMIZE ?= 0

# The CPU reset address as needed by elf2flash
RESET_ADDRESS ?= 0x00004000

#-------------------------------------
# Pre-Initialized Memory Descriptions
#-------------------------------------

# Memory: onchip_memory2_0
MEM_0 := nios_system_onchip_memory2_0
$(MEM_0)_NAME := onchip_memory2_0
$(MEM_0)_MEM_INIT_FILE_PARAM_NAME := INIT_FILE
HEX_FILES += $(MEM_INIT_DIR)/$(MEM_0).hex
MEM_INIT_INSTALL_FILES += $(MEM_INIT_INSTALL_DIR)/$(MEM_0).hex
DAT_FILES += $(HDL_SIM_DIR)/$(MEM_0).dat
HDL_SIM_INSTALL_FILES += $(HDL_SIM_INSTALL_DIR)/$(MEM_0).dat
SYM_FILES += $(HDL_SIM_DIR)/$(MEM_0).sym
HDL_SIM_INSTALL_FILES += $(HDL_SIM_INSTALL_DIR)/$(MEM_0).sym
$(MEM_0)_START := 0x00004000
$(MEM_0)_END := 0x00007fff
$(MEM_0)_HIERARCHICAL_PATH := onchip_memory2_0
$(MEM_0)_WIDTH := 32
$(MEM_0)_ENDIANNESS := --little-endian-mem
$(MEM_0)_CREATE_LANES := 0

.PHONY: onchip_memory2_0
onchip_memory2_0: check_elf_exists $(MEM_INIT_DIR)/$(MEM_0).hex $(HDL_SIM_DIR)/$(MEM_0).dat $(HDL_SIM_DIR)/$(MEM_0).sym


#END OF BSP SPECIFIC

#-------------------------------------
# Pre-Initialized Memory Targets
#-------------------------------------

.PHONY: mem_init_install mem_init_generate mem_init_clean

ifeq ($(QSYS),1)
# Target mem_init_install is deprecated for QSys based systems
# To initialize onchip memories for Quartus II Synthesis with Qsys based systems:
#  1) Use "make mem_init_genearate"
#  2) Add the generated mem_init/meminit.qip file to your Quartus II Project
#
mem_init_install:
	$(error Deprecated Makefile Target: '$@'. Use target 'mem_init_generate' and then add $(MEM_INIT_QIP_FILE) to your Quartus II Project)

else # QSYS != 1, if SopcBuilder based system

ifneq ($(MEM_INIT_INSTALL_DIR),)
mem_init_install: $(MEM_INIT_INSTALL_FILES)
endif

ifneq ($(HDL_SIM_INSTALL_DIR),)
mem_init_install: $(HDL_SIM_INSTALL_FILES)
endif

mem_init_install: mem_init_generate
ifeq ($(MEM_INIT_INSTALL_DIR),)
	@echo "WARNING: MEM_INIT_INSTALL_DIR not set. Set your QUARTUS_PROJECT_DIR environment variable."
endif
ifeq ($(HDL_SIM_INSTALL_DIR),)
	@echo "WARNING: HDL_SIM_INSTALL_DIR not set. Set your QUARTUS_PROJECT_DIR and SOPC_NAME environment variable."
endif

$(MEM_INIT_INSTALL_FILES): $(MEM_INIT_INSTALL_DIR)/%: $(MEM_INIT_DIR)/%
	@$(MKDIR) $(@D)
	@$(CP) -v $< $@

$(HDL_SIM_INSTALL_FILES): $(HDL_SIM_INSTALL_DIR)/%: $(HDL_SIM_DIR)/%
	@$(MKDIR) $(@D)
	@$(CP) -v $< $@

endif # QSYS == 1


mem_init_generate: hex dat sym flash $(MEM_INIT_DESCRIPTOR_FILE) $(MEM_INIT_QIP_FILE)

mem_init_clean:
	@$(RM) -r $(MEM_INIT_DIR) $(HDL_SIM_DIR) $(FLASH_FILES)

.PHONY: hex dat sym flash

hex: check_elf_exists $(HEX_FILES)

dat: check_elf_exists $(DAT_FILES)

sym: check_elf_exists $(SYM_FILES)

flash: check_elf_exists $(FLASH_FILES)

#-------------------------------------
# Pre-Initialized Memory Rules
#-------------------------------------

.PHONY: check_elf_exists
check_elf_exists: $(ELF)
ifeq ($(ELF),)
	$(error ELF var not set in mem_init.mk)
endif

$(filter-out $(FLASH_DAT_FILES),$(DAT_FILES)): %.dat: $(ELF)
	$(post-process-info)
	@$(MKDIR) $(@D)
	bash -c '$(ELF2DAT) --infile=$< --outfile=$@ \
		--base=$(mem_start_address) --end=$(mem_end_address) --width=$(mem_width) \
		$(mem_endianness) --create-lanes=$(mem_create_lanes) $(elf2dat_extra_args)'

$(foreach i,0 1 2 3 4 5 6 7,%_lane$(i).dat): %.dat
	@true

$(HEX_FILES): %.hex: $(ELF)
	$(post-process-info)
	@$(MKDIR) $(@D)
	bash -c '$(ELF2HEX) $< $(mem_start_address) $(mem_end_address) --width=$(mem_width) \
		$(mem_endianness) --create-lanes=$(mem_create_lanes) $(elf2hex_extra_args) $@'

$(SYM_FILES): %.sym: $(ELF)
	$(post-process-info)
	@$(MKDIR) $(@D)
	$(NM) -n $< > $@

$(FLASH_FILES): %.flash: $(ELF)
	$(post-process-info)
	@$(MKDIR) $(@D)
	bash -c '$(ELF2FLASH) --input=$< --outfile=$@ --sim_optimize=$(SIM_OPTIMIZE) $(mem_endianness) \
		$(elf2flash_extra_args)'

#
# Function generate_spd_entry
# Arg1: path to the memory initialization file
# Arg2: Type HEX or DAT
# Arg3: Output spd file to append
gen_spd_entry.BASE_FILE = $(basename $(notdir $1))
gen_spd_entry.PARAM_NAME = $($(gen_spd_entry.BASE_FILE)_MEM_INIT_FILE_PARAM_NAME)
gen_spd_entry.MEM_PATH = $($(gen_spd_entry.BASE_FILE)_HIERARCHICAL_PATH)
gen_spd_entry.SETTINGS = $(strip \
	path=\"$1\" \
	type=\"$2\" \
	$(if $(gen_spd_entry.PARAM_NAME),initParamName=\"$(gen_spd_entry.PARAM_NAME)\") \
	$(if $(gen_spd_entry.MEM_PATH),memoryPath=\"$(gen_spd_entry.MEM_PATH)\") \
)
define gen_spd_entry
$(ECHO) "<file $(gen_spd_entry.SETTINGS) />" >> $3
endef

$(MEM_INIT_DESCRIPTOR_FILE).DAT_FILESET := $(patsubst $(dir $(MEM_INIT_DESCRIPTOR_FILE))%,%,$(DAT_FILES))
$(MEM_INIT_DESCRIPTOR_FILE).HEX_FILESET := $(patsubst $(dir $(MEM_INIT_DESCRIPTOR_FILE))%,%,$(HEX_FILES))

$(MEM_INIT_DESCRIPTOR_FILE): %.spd: $(MEM_INIT_FILE)
	$(post-process-info)
	@$(MKDIR) $(@D)
	@$(RM) $@
	@$(ECHO) "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $@
	@$(ECHO) "<simPackage>" >> $@
	@$(foreach dat_file,$($@.DAT_FILESET),$(call gen_spd_entry,$(dat_file),DAT,$@) &&)true
	@$(foreach hex_file,$($@.HEX_FILESET),$(call gen_spd_entry,$(hex_file),HEX,$@) &&)true
	@$(ECHO) "</simPackage>" >> $@

.DELETE_ON_ERROR: $(MEM_INIT_DESCRIPTOR_FILE)

$(MEM_INIT_QIP_FILE): %.qip: $(MEM_INIT_FILE)
	$(post-process-info)
	@$(MKDIR) $(@D)
	@$(RM) $@
	@$(ECHO) "set_global_assignment -name SEARCH_PATH $$::quartus(qip_path)" > $@

.DELETE_ON_ERROR: $(MEM_INIT_QIP_FILE)
