#------------------------------------------------------------------------------
#              VARIABLES TO BE DEFINED BY PLATFORM MAKEFILE
#------------------------------------------------------------------------------

#  - FPGA_COMMON_DIR     : Path to fpga_common
#  - FIRMWARE_COMMON_DIR : Path to firmware_common
#  - LIBBLADERF_DIR      : Path to libbladeRF
#  - BLADERF_COMMON_DIR  : Path to common bladeRF_nios dir
#  - HOST_COMMON_DIR     : Path to host common directory
#  - NIOS_BUILD_OUTDIR   : Path to place the Nios build products
#  - MEM_INIT_DIR        : Path to place the mem_init build products
#  - BSP_BUILD_OUTDIR    : Path where BSP build products are located
#  - QUARTUS_OUTDIR      : Path where Quartus build products are located
#
# Can opionally define these other variables
#  - ALT_INCLUDE_DIRS
#  - C_SRCS

#------------------------------------------------------------------------------
#              VARIABLES APPENDED TO BY INCLUDED MAKEFILE FRAGMENTS
#------------------------------------------------------------------------------

# List of include directories for -I compiler option (-I added when used).
# Includes the BSP.
ALT_INCLUDE_DIRS += $(BLADERF_COMMON_DIR)/src \
                    $(FIRMWARE_COMMON_DIR) \
                    $(FPGA_COMMON_DIR)/include \
                    $(LIBBLADERF_DIR)/include

# List of library directories for -L linker option (-L added when used).
# Includes the BSP.
ALT_LIBRARY_DIRS :=

# List of library names for -l linker option (-l added when used).
# Includes the BSP.
ALT_LIBRARY_NAMES :=

# List of library names for -msys-lib linker option (-msys-lib added when used).
# These are libraries that might be located in the BSP and depend on the BSP
# library, or vice versa
ALT_BSP_DEP_LIBRARY_NAMES :=

# List of dependencies for the linker.  This is usually the full pathname
# of each library (*.a) file.
# Includes the BSP.
ALT_LDDEPS :=

# List of root library directories that support running make to build them.
# Includes the BSP and any ALT libraries.
MAKEABLE_LIBRARY_ROOT_DIRS :=

# Generic flags passed to the compiler for different types of input files.
ALT_CFLAGS := -DBLADERF_NIOS_BUILD
ALT_CXXFLAGS :=
ALT_CPPFLAGS :=
ALT_ASFLAGS :=
ALT_LDFLAGS :=


#------------------------------------------------------------------------------
#                         The adjust-path macro
#
# If COMSPEC/ComSpec is defined, Make is launched from Windows through
# Cygwin.  The adjust-path macro converts absolute windows paths into
# unix style paths (Example: c:/dir -> /c/dir). This will ensture
# paths are readable by GNU Make.
#
# If COMSPEC/ComSpec is not defined, Make is launched from linux, and no
# adjustment is necessary
#
#------------------------------------------------------------------------------

ifndef COMSPEC
ifdef ComSpec
COMSPEC = $(ComSpec)
endif # ComSpec
endif # COMSPEC

ifdef COMSPEC # if Windows OS

ifeq ($(MAKE_VERSION),3.81)
#
# adjust-path/adjust-path-mixed for Mingw Gnu Make on Windows
#
# Example Usage:
# $(call adjust-path,c:/aaa/bbb) => /c/aaa/bbb
# $(call adjust-path-mixed,/c/aaa/bbb) => c:/aaa/bbb
# $(call adjust-path-mixed,/cygdrive/c/aaa/bbb) => c:/aaa/bbb
#

#
# adjust-path
#  - converts back slash characters into forward slashes
#  - if input arg ($1) is an empty string then return the empty string
#  - if input arg ($1) does not contain the string ":/", then return input arg
#  - using sed, convert mixed path [c:/...] into mingw path [/c/...]
define adjust-path
$(strip \
$(if $1,\
$(if $(findstring :/,$(subst \,/,$1)),\
$(shell echo $(subst \,/,$1) | sed -e 's,^\([a-zA-Z]\):/,/\1/,'),\
$(subst \,/,$1))))
endef

#
# adjust-path-mixed
#  - converts back slash characters into forward slashes
#  - if input arg ($1) is an empty string then return the empty string
#  - if input arg ($1) does not begin with a forward slash '/' char, then
#    return input arg
#  - using sed, convert mingw path [/c/...] or cygwin path [/c/cygdrive/...]
#    into a mixed path [c:/...]
define adjust-path-mixed
$(strip \
$(if $1,\
$(if $(findstring $(subst \,/,$1),$(patsubst /%,%,$(subst \,/,$1))),\
$(subst \,/,$1),\
$(shell echo $(subst \,/,$1) | sed -e 's,^/cygdrive/\([a-zA-Z]\)/,\1:/,' -e 's,^/\([a-zA-Z]\)/,\1:/,'))))
endef

else # MAKE_VERSION != 3.81 (MAKE_VERSION == 3.80 or MAKE_VERSION == 3.79)
#
#  adjust-path for Cygwin Gnu Make
# $(call adjust-path,c:/aaa/bbb) = /cygdrive/c/aaa/bbb
# $(call adjust-path-mixed,/cygdrive/c/aaa/bbb) = c:/aaa/bbb
#
adjust-path = $(if $1,$(shell cygpath -u "$1"),)
adjust-path-mixed = $(if $1,$(shell cygpath -m "$1"),)
endif

else # !COMSPEC

adjust-path = $1
adjust-path-mixed = $1

endif # COMSPEC


#vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
#                           GENERATED SETTINGS START                         v
#vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

#START GENERATED
ACTIVE_BUILD_CONFIG := default
BUILD_CONFIGS := default

# The following TYPE comment allows tools to identify the 'type' of target this
# makefile is associated with.
# TYPE: APP_MAKEFILE

# This following VERSION comment indicates the version of the tool used to
# generate this makefile. A makefile variable is provided for VERSION as well.
# ACDS_VERSION: 12.1sp1
ACDS_VERSION := 12.1sp1

# This following BUILD_NUMBER comment indicates the build number of the tool
# used to generate this makefile.
# BUILD_NUMBER: 243

# Define path to the application ELF.
# It may be used by the makefile fragments so is defined before including them.
#
ELF := $(NIOS_BUILD_OUTDIR)/bladeRF_nios.elf

# Paths to C, C++, and assembly source files.
C_SRCS += $(BLADERF_COMMON_DIR)/src/devices.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_8x8.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_8x16.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_8x32.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_8x64.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_16x64.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_32x32.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/pkt_legacy.c
C_SRCS += $(BLADERF_COMMON_DIR)/src/devices_sim.c
CXX_SRCS :=
ASM_SRCS :=


# Path to root of object file tree.
OBJ_ROOT_DIR := $(NIOS_BUILD_OUTDIR)/obj

# Options to control objdump.
CREATE_OBJDUMP := 1
OBJDUMP_INCLUDE_SOURCE := 1
OBJDUMP_FULL_CONTENTS := 0

# Options to enable/disable optional files.
CREATE_ELF_DERIVED_FILES := 0
CREATE_LINKER_MAP := 1

# Common arguments for ALT_CFLAGSs
APP_CFLAGS_DEFINED_SYMBOLS := -DNUAND_MODIFICATIONS
APP_CFLAGS_UNDEFINED_SYMBOLS :=
APP_CFLAGS_OPTIMIZATION := -Os -fshort-enums
APP_CFLAGS_DEBUG_LEVEL := -g
APP_CFLAGS_WARNINGS := -Wall -Wextra -Wno-unused-parameter -Werror
APP_CFLAGS_USER_FLAGS :=

APP_ASFLAGS_USER :=
APP_LDFLAGS_USER :=

# Linker options that have default values assigned later if not
# assigned here.
LINKER_SCRIPT :=
CRT0 :=
SYS_LIB :=

# Define path to the root of the BSP.
BSP_ROOT_DIR := $(BSP_BUILD_OUTDIR)

# List of application specific include directories, library directories and library names
APP_INCLUDE_DIRS :=
APP_LIBRARY_DIRS :=
APP_LIBRARY_NAMES :=

# Pre- and post- processor settings.
BUILD_PRE_PROCESS :=
BUILD_POST_PROCESS :=

QUARTUS_PROJECT_DIR := $(QUARTUS_OUTDIR)


#END GENERATED

#^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#                            GENERATED SETTINGS END                           ^
#^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


#------------------------------------------------------------------------------
#                           DEFAULT TARGET
#------------------------------------------------------------------------------

# Define the variable used to echo output if not already defined.
ifeq ($(ECHO),)
ECHO := echo
endif

# Put "all" rule before included makefile fragments because they may
# define rules and we don't want one of those to become the default rule.
.PHONY : all

all:
	@$(ECHO) [$(APP_NAME) build complete]

all : build_pre_process libs app build_post_process


#------------------------------------------------------------------------------
#                 VARIABLES DEPENDENT ON GENERATED CONTENT
#------------------------------------------------------------------------------

# Define object file directory per build configuration
CONFIG_OBJ_DIR := $(OBJ_ROOT_DIR)/$(ACTIVE_BUILD_CONFIG)

ifeq ($(BSP_ROOT_DIR),)
$(error Edit Makefile and provide a value for BSP_ROOT_DIR)
endif

ifeq ($(wildcard $(BSP_ROOT_DIR)),)
$(error BSP directory does not exist: $(BSP_ROOT_DIR))
endif

# Define absolute path to the root of the BSP.
ABS_BSP_ROOT_DIR := $(call adjust-path-mixed,$(shell cd "$(BSP_ROOT_DIR)"; pwd))

# Include makefile fragments.  Define variable ALT_LIBRARY_ROOT_DIR before
# including each makefile fragment so that it knows the path to itself.
BSP_INCLUDE_FILE := $(BSP_ROOT_DIR)/public.mk
ALT_LIBRARY_ROOT_DIR := $(BSP_ROOT_DIR)
include $(BSP_INCLUDE_FILE)
# C2H will need this to touch the BSP public.mk and avoid the sopc file
# out-of-date error during a BSP make
ABS_BSP_INCLUDE_FILE := $(ABS_BSP_ROOT_DIR)/public.mk


ifneq ($(WARNING.SMALL_STACK_SIZE),)
# This WARNING is here to protect you from unknowingly using a very small stack
# If the warning is set, increase your stack size or enable the BSP small stack
# setting to eliminate the warning
$(warning WARNING: $(WARNING.SMALL_STACK_SIZE))
endif


# If the BSP public.mk indicates that ALT_SIM_OPTIMIZE is set, rename the ELF
# by prefixing it with RUN_ON_HDL_SIMULATOR_ONLY_.
ifneq ($(filter -DALT_SIM_OPTIMIZE,$(ALT_CPPFLAGS)),)
ELF := RUN_ON_HDL_SIMULATOR_ONLY_$(ELF)
endif

# If the BSP public.mk indicates that ALT_PROVIDE_GMON is set, add option to
# download_elf target
ifneq ($(filter -DALT_PROVIDE_GMON,$(ALT_CPPFLAGS)),)
GMON_OUT_FILENAME := gmon.out
WRITE_GMON_OPTION := --write-gmon $(GMON_OUT_FILENAME)
endif

# Name of ELF application.
APP_NAME := $(basename $(ELF))

# Set to defaults if variables not already defined in settings.
ifeq ($(LINKER_SCRIPT),)
LINKER_SCRIPT := $(BSP_LINKER_SCRIPT)
endif
ifeq ($(CRT0),)
CRT0 := $(BSP_CRT0)
endif
ifeq ($(SYS_LIB),)
SYS_LIB := $(BSP_SYS_LIB)
endif

OBJDUMP_NAME := $(APP_NAME).objdump
OBJDUMP_FLAGS := --disassemble --syms --all-header
ifeq ($(OBJDUMP_INCLUDE_SOURCE),1)
OBJDUMP_FLAGS += --source
endif
ifeq ($(OBJDUMP_FULL_CONTENTS),1)
OBJDUMP_FLAGS += --full-contents
endif

# Create list of linker dependencies (*.a files).
APP_LDDEPS := $(ALT_LDDEPS) $(LDDEPS)

# Take lists and add required prefixes.
APP_INC_DIRS := $(addprefix -I, $(ALT_INCLUDE_DIRS) $(APP_INCLUDE_DIRS) $(INC_DIRS))
ASM_INC_PREFIX := -Wa,-I
APP_ASM_INC_DIRS := $(addprefix $(ASM_INC_PREFIX), $(ALT_INCLUDE_DIRS) $(APP_INCLUDE_DIRS) $(INC_DIRS))
APP_LIB_DIRS := $(addprefix -L, $(ALT_LIBRARY_DIRS) $(APP_LIBRARY_DIRS) $(LIB_DIRS))
APP_LIBS := $(addprefix -l, $(ALT_LIBRARY_NAMES) $(APP_LIBRARY_NAMES) $(LIBS))

ifneq ($(AVOID_NIOS2_GCC3_OPTIONS),)

#
# Avoid Nios II GCC 3.X options.
#

# Detect if small newlib C library is requested.
# If yes, remove the -msmallc option because it is
# now handled by other means.
ifneq ($(filter -msmallc,$(ALT_LDFLAGS)),)
	ALT_LDFLAGS := $(filter-out -msmallc,$(ALT_LDFLAGS))
	ALT_C_LIBRARY := smallc
else
	ALT_C_LIBRARY := c
endif

# Put each BSP dependent library in a group to avoid circular dependencies.
APP_BSP_DEP_LIBS := $(foreach l,$(ALT_BSP_DEP_LIBRARY_NAMES),-Wl,--start-group -l$(ALT_C_LIBRARY) -lgcc -l$(l) -Wl,--end-group)

else # !AVOID_NIOS2_GCC3_OPTIONS

#
# Use Nios II GCC 3.X options.
#
APP_BSP_DEP_LIBS := $(addprefix -msys-lib=, $(ALT_BSP_DEP_LIBRARY_NAMES))

endif # !AVOID_NIOS2_GCC3_OPTIONS

# Arguments for the C preprocessor, C/C++ compiler, assembler, and linker.
APP_CFLAGS := $(APP_CFLAGS_DEFINED_SYMBOLS) \
              $(APP_CFLAGS_UNDEFINED_SYMBOLS) \
              $(APP_CFLAGS_OPTIMIZATION) \
              $(APP_CFLAGS_DEBUG_LEVEL) \
              $(APP_CFLAGS_WARNINGS) \
              $(APP_CFLAGS_USER_FLAGS) \
              $(ALT_CFLAGS) \
              $(CFLAGS)

# Arguments only for the C++ compiler.
APP_CXXFLAGS := $(ALT_CXXFLAGS) $(CXXFLAGS)

# Arguments only for the C preprocessor.
# Prefix each include directory with -I.
APP_CPPFLAGS := $(APP_INC_DIRS) \
                $(ALT_CPPFLAGS) \
                $(CPPFLAGS)

# Arguments only for the assembler.
APP_ASFLAGS := $(APP_ASM_INC_DIRS) \
               $(ALT_ASFLAGS) \
               $(APP_ASFLAGS_USER) \
               $(ASFLAGS)

# Arguments only for the linker.
APP_LDFLAGS := $(APP_LDFLAGS_USER)

ifneq ($(LINKER_SCRIPT),)
APP_LDFLAGS += -T'$(LINKER_SCRIPT)'
endif

ifneq ($(AVOID_NIOS2_GCC3_OPTIONS),)

# Avoid Nios II GCC 3.x options.
ifneq ($(CRT0),)
APP_LDFLAGS += $(CRT0)
endif

# The equivalent of the -msys-lib option is provided
# by the GROUP() command in the linker script.
# Note this means the SYS_LIB variable is now ignored.

else # !AVOID_NIOS2_GCC3_OPTIONS

# Use Nios II GCC 3.x options.
ifneq ($(CRT0),)
APP_LDFLAGS += -msys-crt0='$(CRT0)'
endif
ifneq ($(SYS_LIB),)
APP_LDFLAGS += -msys-lib=$(SYS_LIB)
endif

endif # !AVOID_NIOS2_GCC3_OPTIONS

APP_LDFLAGS += \
           $(APP_LIB_DIRS) \
           $(ALT_LDFLAGS) \
           $(LDFLAGS)

LINKER_MAP_NAME := $(APP_NAME).map
ifeq ($(CREATE_LINKER_MAP), 1)
APP_LDFLAGS += -Wl,-Map=$(LINKER_MAP_NAME)
endif

# QUARTUS_PROJECT_DIR and SOPC_NAME need to be defined if you want the
# mem_init_install target of the mem_init.mk (located in the associated BSP)
# to know how to copy memory initialization files (e.g. .dat, .hex) into
# directories required for Quartus compilation or RTL simulation.

# Defining QUARTUS_PROJECT_DIR causes mem_init_install to copy memory
# initialization files into your Quartus project directory. This is required
# to provide the initial memory contents of FPGA memories that can be
# initialized by the programming file (.sof) or Hardcopy ROMs. It is also used
# for VHDL simulation of on-chip memories.

# Defining SOPC_NAME causes the mem_init_install target to copy memory
# initialization files into your RTL simulation directory.  This is required
# to provide the initial memory contents of all memories that can be
# initialized by RTL simulation. This variable should be set to the same name
# as your SOPC Builder system name. For example, if you have a system called
# "foo.sopc", this variable should be set to "foo".

# If SOPC_NAME is not set and QUARTUS_PROJECT_DIR is set, then derive SOPC_NAME.
ifeq ($(SOPC_NAME),)
ifneq ($(QUARTUS_PROJECT_DIR),)
SOPC_NAME := $(basename $(notdir $(wildcard $(QUARTUS_PROJECT_DIR)/*.sopcinfo)))
endif
endif

# Defining JDI_FILE is required to specify the JTAG Debug Information File
# path. This file is generated by Quartus, and is needed along with the
# .sopcinfo file to resolve processor instance ID's from names in a multi-CPU
# systems. For multi-CPU systems, the processor instance ID is used to select
# from multiple CPU's during ELF download.

# Both JDI_FILE and SOPCINFO_FILE are provided by the BSP if they found during
# BSP creation. If JDI_FILE is not set and QUARTUS_PROJECT_DIR is set, then
# derive JDI_FILE. We do not attempt to derive SOPCINFO_FILE since there may be
# multiple .sopcinfo files in a Quartus project.
ifeq ($(JDI_FILE),)
ifneq ($(QUARTUS_PROJECT_DIR),)
JDI_FILE := $(wildcard $(QUARTUS_PROJECT_DIR)/*.jdi)
endif
endif

# Path to root runtime directory used for hdl simulation
RUNTIME_ROOT_DIR := $(CONFIG_OBJ_DIR)/runtime



#------------------------------------------------------------------------------
#           MAKEFILE INCLUDES DEPENDENT ON GENERATED CONTENT
#------------------------------------------------------------------------------
# mem_init.mk is a generated makefile fragment. This file defines all targets
# used to generate HDL initialization simulation files and pre-initialized
# onchip memory files.
MEM_INIT_FILE :=  $(BSP_ROOT_DIR)/mem_init.mk
include $(MEM_INIT_FILE)

# Create list of object files to be built using the list of source files.
# The source file hierarchy is preserved in the object tree.
# The supported file extensions are:
#
# .c            - for C files
# .cxx .cc .cpp - for C++ files
# .S .s         - for assembler files
#
# Handle source files specified by --src-dir & --src-rdir differently, to
# save some processing time in calling the adjust-path macro.

OBJ_LIST_C 		:= $(patsubst %.c,%.o,$(filter %.c,$(C_SRCS)))
OBJ_LIST_CPP	:= $(patsubst %.cpp,%.o,$(filter %.cpp,$(CXX_SRCS)))
OBJ_LIST_CXX 	:= $(patsubst %.cxx,%.o,$(filter %.cxx,$(CXX_SRCS)))
OBJ_LIST_CC 	:= $(patsubst %.cc,%.o,$(filter %.cc,$(CXX_SRCS)))
OBJ_LIST_S 		:= $(patsubst %.S,%.o,$(filter %.S,$(ASM_SRCS)))
OBJ_LIST_SS		:= $(patsubst %.s,%.o,$(filter %.s,$(ASM_SRCS)))

OBJ_LIST := $(sort $(OBJ_LIST_C) $(OBJ_LIST_CPP) $(OBJ_LIST_CXX) \
				$(OBJ_LIST_CC) $(OBJ_LIST_S) $(OBJ_LIST_SS))

SDIR_OBJ_LIST_C		:= $(patsubst %.c,%.o,$(filter %.c,$(SDIR_C_SRCS)))
SDIR_OBJ_LIST_CPP	:= $(patsubst %.cpp,%.o,$(filter %.cpp,$(SDIR_CXX_SRCS)))
SDIR_OBJ_LIST_CXX 	:= $(patsubst %.cxx,%.o,$(filter %.cxx,$(SDIR_CXX_SRCS)))
SDIR_OBJ_LIST_CC 	:= $(patsubst %.cc,%.o,$(filter %.cc,$(SDIR_CXX_SRCS)))
SDIR_OBJ_LIST_S		:= $(patsubst %.S,%.o,$(filter %.S,$(SDIR_ASM_SRCS)))
SDIR_OBJ_LIST_SS	:= $(patsubst %.s,%.o,$(filter %.s,$(SDIR_ASM_SRCS)))

SDIR_OBJ_LIST := $(sort $(SDIR_OBJ_LIST_C) $(SDIR_OBJ_LIST_CPP) \
				$(SDIR_OBJ_LIST_CXX) $(SDIR_OBJ_LIST_CC) $(SDIR_OBJ_LIST_S) \
				$(SDIR_OBJ_LIST_SS))

# Relative-pathed objects that being with "../" are handled differently.
#
# Regular objects are created as
#   $(CONFIG_OBJ_DIR)/<path>/<filename>.o
# where the path structure is maintained under the obj directory.  This
# applies for both absolute and relative paths; in the absolute path
# case this means the entire source path will be recreated under the obj
# directory.  This is done to allow two source files with the same name
# to be included as part of the project.
#
# Note: On Cygwin, the path recreated under the obj directory will be
# the cygpath -u output path.
#
# Relative-path objects that begin with "../" cause problems under this
# scheme, as $(CONFIG_OBJ_DIR)/../<rest of path>/ can potentially put the object
# files anywhere in the system, creating clutter and polluting the source tree.
# As such, their paths are flattened - the object file created will be
# $(CONFIG_OBJ_DIR)/<filename>.o.  Due to this, two files specified with
# "../" in the beginning cannot have the same name in the project.  VPATH
# will be set for these sources to allow make to relocate the source file
# via %.o rules.
#
# The following lines separate the object list into the flatten and regular
# lists, and then handles them as appropriate.

FLATTEN_OBJ_LIST := $(filter ../%,$(OBJ_LIST))
FLATTEN_APP_OBJS := $(addprefix $(CONFIG_OBJ_DIR)/,$(notdir $(FLATTEN_OBJ_LIST)))

REGULAR_OBJ_LIST 		:= $(filter-out $(FLATTEN_OBJ_LIST),$(OBJ_LIST))
REGULAR_OBJ_LIST_C 		:= $(filter $(OBJ_LIST_C),$(REGULAR_OBJ_LIST))
REGULAR_OBJ_LIST_CPP	:= $(filter $(OBJ_LIST_CPP),$(REGULAR_OBJ_LIST))
REGULAR_OBJ_LIST_CXX 	:= $(filter $(OBJ_LIST_CXX),$(REGULAR_OBJ_LIST))
REGULAR_OBJ_LIST_CC 	:= $(filter $(OBJ_LIST_CC),$(REGULAR_OBJ_LIST))
REGULAR_OBJ_LIST_S 		:= $(filter $(OBJ_LIST_S),$(REGULAR_OBJ_LIST))
REGULAR_OBJ_LIST_SS		:= $(filter $(OBJ_LIST_SS),$(REGULAR_OBJ_LIST))

FLATTEN_SDIR_OBJ_LIST := $(filter ../%,$(SDIR_OBJ_LIST))
FLATTEN_SDIR_APP_OBJS := $(addprefix $(CONFIG_OBJ_DIR)/,$(notdir $(FLATTEN_SDIR_OBJ_LIST)))

REGULAR_SDIR_OBJ_LIST 		:= $(filter-out $(FLATTEN_SDIR_OBJ_LIST),$(SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_C 	:= $(filter $(SDIR_OBJ_LIST_C),$(REGULAR_SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_CPP	:= $(filter $(SDIR_OBJ_LIST_CPP),$(REGULAR_SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_CXX 	:= $(filter $(SDIR_OBJ_LIST_CXX),$(REGULAR_SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_CC 	:= $(filter $(SDIR_OBJ_LIST_CC),$(REGULAR_SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_S 	:= $(filter $(SDIR_OBJ_LIST_S),$(REGULAR_SDIR_OBJ_LIST))
REGULAR_SDIR_OBJ_LIST_SS	:= $(filter $(SDIR_OBJ_LIST_SS),$(REGULAR_SDIR_OBJ_LIST))

VPATH := $(sort $(dir $(FLATTEN_OBJ_LIST)) $(dir $(FLATTEN_SDIR_OBJ_LIST)))

APP_OBJS_C := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_C) \
	$(foreach s,$(REGULAR_OBJ_LIST_C),$(call adjust-path,$s)))

APP_OBJS_CPP := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_CPP) \
	$(foreach s,$(REGULAR_OBJ_LIST_CPP),$(call adjust-path,$s)))

APP_OBJS_CXX := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_CXX) \
	$(foreach s,$(REGULAR_OBJ_LIST_CXX),$(call adjust-path,$s)))

APP_OBJS_CC := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_CC) \
	$(foreach s,$(REGULAR_OBJ_LIST_CC),$(call adjust-path,$s)))

APP_OBJS_S := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_S) \
	$(foreach s,$(REGULAR_OBJ_LIST_S),$(call adjust-path,$s)))

APP_OBJS_SS := $(addprefix $(CONFIG_OBJ_DIR)/,\
	$(REGULAR_SDIR_OBJ_LIST_SS) \
	$(foreach s,$(REGULAR_OBJ_LIST_SS),$(call adjust-path,$s)))

APP_OBJS := $(APP_OBJS_C) $(APP_OBJS_CPP) $(APP_OBJS_CXX) $(APP_OBJS_CC) \
	$(APP_OBJS_S) $(APP_OBJS_SS) \
	$(FLATTEN_APP_OBJS) $(FLATTEN_SDIR_APP_OBJS)

# Add any extra user-provided object files.
APP_OBJS += $(OBJS)

# Create list of dependancy files for each object file.
APP_DEPS := $(APP_OBJS:.o=.d)

# Patch the Elf file with system specific information

# Patch the Elf with the name of the sopc system
ifneq ($(SOPC_NAME),)
ELF_PATCH_FLAG += --sopc_system_name $(SOPC_NAME)
endif

# Patch the Elf with the absolute path to the Quartus Project Directory
ifneq ($(QUARTUS_PROJECT_DIR),)
ABS_QUARTUS_PROJECT_DIR := $(call adjust-path-mixed,$(shell cd "$(QUARTUS_PROJECT_DIR)"; pwd))
ELF_PATCH_FLAG += --quartus_project_dir "$(ABS_QUARTUS_PROJECT_DIR)"
endif

# Patch the Elf and download args with the JDI_FILE if specified
ifneq ($(wildcard $(JDI_FILE)),)
ELF_PATCH_FLAG += --jdi $(JDI_FILE)
DOWNLOAD_JDI_FLAG := --jdi $(JDI_FILE)
endif

# Patch the Elf with the SOPCINFO_FILE if specified
ifneq ($(wildcard $(SOPCINFO_FILE)),)
ELF_PATCH_FLAG += --sopcinfo $(SOPCINFO_FILE)
endif

# Use the DOWNLOAD_CABLE variable to specify which JTAG cable to use.
# This is not needed if you only have one cable.
ifneq ($(DOWNLOAD_CABLE),)
DOWNLOAD_CABLE_FLAG := --cable '$(DOWNLOAD_CABLE)'
endif


#------------------------------------------------------------------------------
#                           BUILD PRE/POST PROCESS
#------------------------------------------------------------------------------
build_pre_process :
	$(BUILD_PRE_PROCESS)

build_post_process :
	$(BUILD_POST_PROCESS)

.PHONY: build_pre_process build_post_process


#------------------------------------------------------------------------------
#                                 TOOLS
#------------------------------------------------------------------------------

#
# Set tool default variables if not already defined.
# If these are defined, they would typically be defined in an
# included makefile fragment.
#
ifeq ($(DEFAULT_CROSS_COMPILE),)
DEFAULT_CROSS_COMPILE := nios2-elf-
endif

ifeq ($(DEFAULT_STACK_REPORT),)
DEFAULT_STACKREPORT := nios2-stackreport
endif

ifeq ($(DEFAULT_DOWNLOAD),)
DEFAULT_DOWNLOAD := nios2-download
endif

ifeq ($(DEFAULT_FLASHPROG),)
DEFAULT_FLASHPROG := nios2-flash-programmer
endif

ifeq ($(DEFAULT_ELFPATCH),)
DEFAULT_ELFPATCH := nios2-elf-insert
endif

ifeq ($(DEFAULT_RM),)
DEFAULT_RM := rm -f
endif

ifeq ($(DEFAULT_CP),)
DEFAULT_CP := cp -f
endif

ifeq ($(DEFAULT_MKDIR),)
DEFAULT_MKDIR := mkdir -p
endif

#
# Set tool variables to defaults if not already defined.
# If these are defined, they would typically be defined by a
# setting in the generated portion of this makefile.
#
ifeq ($(CROSS_COMPILE),)
CROSS_COMPILE := $(DEFAULT_CROSS_COMPILE)
endif

ifeq ($(origin CC),default)
CC := $(CROSS_COMPILE)gcc -xc
endif

ifeq ($(origin CXX),default)
CXX := $(CROSS_COMPILE)gcc -xc++
endif

ifeq ($(origin AS),default)
AS := $(CROSS_COMPILE)gcc
endif

ifeq ($(origin AR),default)
AR := $(CROSS_COMPILE)ar
endif

ifeq ($(origin LD),default)
LD := $(CROSS_COMPILE)g++
endif

ifeq ($(origin NM),default)
NM := $(CROSS_COMPILE)nm
endif

ifeq ($(origin RM),default)
RM := $(DEFAULT_RM)
endif

ifeq ($(origin CP),default)
CP := $(DEFAULT_CP)
endif

ifeq ($(OBJDUMP),)
OBJDUMP := $(CROSS_COMPILE)objdump
endif

ifeq ($(OBJCOPY),)
OBJCOPY := $(CROSS_COMPILE)objcopy
endif

ifeq ($(STACKREPORT),)
ifeq ($(CROSS_COMPILE),nios2-elf-)
STACKREPORT := $(DEFAULT_STACKREPORT)
else
DISABLE_STACKREPORT := 1
endif
endif

ifeq ($(DOWNLOAD),)
DOWNLOAD := $(DEFAULT_DOWNLOAD)
endif

ifeq ($(FLASHPROG),)
FLASHPROG := $(DEFAULT_FLASHPROG)
endif

ifeq ($(ELFPATCH),)
ELFPATCH := $(DEFAULT_ELFPATCH)
endif

ifeq ($(MKDIR),)
MKDIR := $(DEFAULT_MKDIR)
endif

#------------------------------------------------------------------------------
#                     PATTERN RULES TO BUILD OBJECTS
#------------------------------------------------------------------------------

define compile.c
@$(ECHO) Info: Compiling $< to $@
@$(MKDIR) $(@D)
$(CC) -MP -MMD -c $(APP_CPPFLAGS) $(APP_CFLAGS) -o $@ $<
$(CC_POST_PROCESS)
endef

define compile.cpp
@$(ECHO) Info: Compiling $< to $@
@$(MKDIR) $(@D)
$(CXX) -MP -MMD -c $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<
$(CXX_POST_PROCESS)
endef

# If assembling with the compiler, ensure "-Wa," is prepended to all APP_ASFLAGS
ifeq ($(AS),$(patsubst %as,%,$(AS)))
COMMA := ,
APP_ASFLAGS :=  $(filter-out $(APP_CFLAGS),$(addprefix -Wa$(COMMA),$(patsubst -Wa$(COMMA)%,%,$(APP_ASFLAGS))))
endif

define compile.s
@$(ECHO) Info: Assembling $< to $@
@$(MKDIR) $(@D)
$(AS) -MP -MMD -c $(APP_CPPFLAGS) $(APP_CFLAGS) $(APP_ASFLAGS) -o $@ $<
$(AS_POST_PROCESS)
endef

ifeq ($(MAKE_VERSION),3.81)
.SECONDEXPANSION:

$(APP_OBJS_C): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.c)
	$(compile.c)

$(APP_OBJS_CPP): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.cpp)
	$(compile.cpp)

$(APP_OBJS_CC): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.cc)
	$(compile.cpp)

$(APP_OBJS_CXX): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.cxx)
	$(compile.cpp)

$(APP_OBJS_S): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.S)
	$(compile.s)

$(APP_OBJS_SS): $(CONFIG_OBJ_DIR)/%.o: $$(call adjust-path-mixed,%.s)
	$(compile.s)

endif # MAKE_VERSION != 3.81

$(CONFIG_OBJ_DIR)/%.o: %.c
	$(compile.c)

$(CONFIG_OBJ_DIR)/%.o: %.cpp
	$(compile.cpp)

$(CONFIG_OBJ_DIR)/%.o: %.cc
	$(compile.cpp)

$(CONFIG_OBJ_DIR)/%.o: %.cxx
	$(compile.cpp)

$(CONFIG_OBJ_DIR)/%.o: %.S
	$(compile.s)

$(CONFIG_OBJ_DIR)/%.o: %.s
	$(compile.s)


#------------------------------------------------------------------------------
#                     PATTERN RULES TO INTERMEDIATE FILES
#------------------------------------------------------------------------------

$(CONFIG_OBJ_DIR)/%.s: %.c
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CC) -S $(APP_CPPFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.s: %.cpp
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -S $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.s: %.cc
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -S $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.s: %.cxx
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -S $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.i: %.c
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CC) -E $(APP_CPPFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.i: %.cpp
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -E $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.i: %.cc
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -E $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<

$(CONFIG_OBJ_DIR)/%.i: %.cxx
	@$(ECHO) Info: Compiling $< to $@
	@$(MKDIR) $(@D)
	$(CXX) -E $(APP_CPPFLAGS) $(APP_CXXFLAGS) $(APP_CFLAGS) -o $@ $<


#------------------------------------------------------------------------------
#                        TARGET RULES
#------------------------------------------------------------------------------

.PHONY : help
help :
	@$(ECHO) "Summary of Makefile targets"
	@$(ECHO) "  Build targets:"
	@$(ECHO) "    all (default)     - Application and all libraries (including BSP)"
	@$(ECHO) "    bsp               - Just the BSP"
	@$(ECHO) "    libs              - All libraries (including BSP)"
	@$(ECHO) "    flash             - All flash files"
	@$(ECHO) "    mem_init_generate - All memory initialization files"
ifeq ($(QSYS),1)
	@$(ECHO) "    mem_init_install  - This target is deprecated for QSys Systems"
	@$(ECHO) "                         --> Use the mem_init_generate target and then"
	@$(ECHO) "                             add the generated meminit.qip file to your"
	@$(ECHO) "                             Quartus II Project."
else # if QSYS != 1
	@$(ECHO) "    mem_init_install  - Copy memory initialization files to Quartus II project"
endif # QSYS == 1
	@$(ECHO)
	@$(ECHO) "  Clean targets:"
	@$(ECHO) "    clean_all         - Application and all libraries (including BSP)"
	@$(ECHO) "    clean             - Just the application"
	@$(ECHO) "    clean_bsp         - Just the BSP"
	@$(ECHO) "    clean_libs        - All libraries (including BSP)"
	@$(ECHO)
	@$(ECHO) "  Run targets:"
	@$(ECHO) "    download-elf      - Download and run your elf executable"
	@$(ECHO) "    program-flash     - Program flash contents to the board"

# Handy rule to skip making libraries and just make application.
.PHONY : app
app : $(ELF)

ifeq ($(CREATE_OBJDUMP), 1)
app : $(OBJDUMP_NAME)
endif

ifeq ($(CREATE_ELF_DERIVED_FILES),1)
app : elf_derived_files
endif

.PHONY: elf_derived_files
elf_derived_files: default_mem_init

# Handy rule for making just the BSP.
.PHONY : bsp
bsp :
	@$(ECHO) Info: Building $(BSP_ROOT_DIR)
	@$(MAKE) --no-print-directory -C $(BSP_ROOT_DIR)


# Make sure all makeable libraries (including the BSP) are up-to-date.
LIB_TARGETS := $(patsubst %,%-recurs-make-lib,$(MAKEABLE_LIBRARY_ROOT_DIRS))

.PHONY : libs
libs : $(LIB_TARGETS)

ifneq ($(strip $(LIB_TARGETS)),)
$(LIB_TARGETS): %-recurs-make-lib:
	@$(ECHO) Info: Building $*
	$(MAKE) --no-print-directory -C $*
endif

ifneq ($(strip $(APP_LDDEPS)),)
$(APP_LDDEPS): libs
	@true
endif

# Rules to force your project to rebuild or relink
# .force_relink file will cause any application that depends on this project to relink
# .force_rebuild file will cause this project to rebuild object files
# .force_rebuild_all file will cause this project and any project that depends on this project to rebuild object files

FORCE_RELINK_DEP  := .force_relink
FORCE_REBUILD_DEP := .force_rebuild
FORCE_REBUILD_ALL_DEP := .force_rebuild_all
FORCE_REBUILD_DEP_LIST := $(CONFIG_OBJ_DIR)/$(FORCE_RELINK_DEP) $(CONFIG_OBJ_DIR)/$(FORCE_REBUILD_DEP) $(FORCE_REBUILD_ALL_DEP)

$(FORCE_REBUILD_DEP_LIST):

$(APP_OBJS): $(wildcard $(CONFIG_OBJ_DIR)/$(FORCE_REBUILD_DEP)) $(wildcard $(addsuffix /$(FORCE_REBUILD_ALL_DEP), . $(ALT_LIBRARY_DIRS)))

$(ELF): $(wildcard $(addsuffix /$(FORCE_RELINK_DEP), $(CONFIG_OBJ_DIR) $(ALT_LIBRARY_DIRS)))


# Clean just the application.
.PHONY : clean
ifeq ($(CREATE_ELF_DERIVED_FILES),1)
clean : clean_elf_derived_files
endif

clean :
	@$(RM) -r $(ELF) $(OBJDUMP_NAME) $(LINKER_MAP_NAME) $(OBJ_ROOT_DIR) $(RUNTIME_ROOT_DIR) $(FORCE_REBUILD_DEP_LIST)
	@$(ECHO) [$(APP_NAME) clean complete]

# Clean just the BSP.
.PHONY : clean_bsp
clean_bsp :
	@$(ECHO) Info: Cleaning $(BSP_ROOT_DIR)
	@$(MAKE) --no-print-directory -C $(BSP_ROOT_DIR) clean

# Clean all makeable libraries including the BSP.
LIB_CLEAN_TARGETS := $(patsubst %,%-recurs-make-clean-lib,$(MAKEABLE_LIBRARY_ROOT_DIRS))

.PHONY : clean_libs
clean_libs : $(LIB_CLEAN_TARGETS)

ifneq ($(strip $(LIB_CLEAN_TARGETS)),)
$(LIB_CLEAN_TARGETS): %-recurs-make-clean-lib:
	@$(ECHO) Info: Cleaning $*
	$(MAKE) --no-print-directory -C $* clean
endif

.PHONY: clean_elf_derived_files
clean_elf_derived_files: mem_init_clean

# Clean application and all makeable libraries including the BSP.
.PHONY : clean_all
clean_all : clean mem_init_clean clean_libs

# Include the dependency files unless the make goal is performing a clean
# of the application.
ifneq ($(firstword $(MAKECMDGOALS)),clean)
ifneq ($(firstword $(MAKECMDGOALS)),clean_all)
-include $(APP_DEPS)
endif
endif

.PHONY : download-elf
download-elf : $(ELF)
	@if [ "$(DOWNLOAD)" = "none" ]; \
	then \
		$(ECHO) Downloading $(ELF) not supported; \
	else \
		$(ECHO) Info: Downloading $(ELF); \
		$(DOWNLOAD) --go --cpu_name=$(CPU_NAME) $(DOWNLOAD_CABLE_FLAG) $(SOPC_SYSID_FLAG) $(DOWNLOAD_JDI_FLAG) $(WRITE_GMON_OPTION) $(ELF); \
	fi

# Delete the target of a rule if it has changed and its commands exit
# with a nonzero exit status.
.DELETE_ON_ERROR:

# Rules for flash programming commands
PROGRAM_FLASH_SUFFIX := -program
PROGRAM_FLASH_TARGET := $(addsuffix $(PROGRAM_FLASH_SUFFIX), $(FLASH_FILES))

.PHONY : program-flash
program-flash : $(PROGRAM_FLASH_TARGET)

.PHONY : $(PROGRAM_FLASH_TARGET)
$(PROGRAM_FLASH_TARGET) : flash
	@if [ "$(FLASHPROG)" = "none" ]; \
	then \
		$(ECHO) Programming flash not supported; \
	else \
		$(ECHO) Info: Programming $(basename $@).flash; \
		if [ -z "$($(basename $@)_EPCS_FLAGS)" ]; \
		then \
			$(ECHO) $(FLASHPROG) $(SOPC_SYSID_FLAG) --base=$($(basename $@)_START) $(basename $@).flash; \
			$(FLASHPROG) $(DOWNLOAD_CABLE_FLAG) $(SOPC_SYSID_FLAG) --base=$($(basename $@)_START) $(basename $@).flash; \
		else \
			$(ECHO) $(FLASHPROG) $(SOPC_SYSID_FLAG) --epcs --base=$($(basename $@)_START) $(basename $@).flash; \
			$(FLASHPROG) $(DOWNLOAD_CABLE_FLAG) $(SOPC_SYSID_FLAG) --epcs --base=$($(basename $@)_START) $(basename $@).flash; \
		fi \
	fi


# Rules for simulating with an HDL Simulator [QSYS only]
ifeq ($(QSYS),1)
IP_MAKE_SIMSCRIPT := ip-make-simscript

ifeq ($(VSIM),)
VSIM_EXE := $(if $(VSIM_DIR),$(VSIM_DIR)/,)vsim
ifeq ($(ENABLE_VSIM_GUI),1)
VSIM := $(VSIM_EXE) -gui
else
VSIM := $(VSIM_EXE) -c
endif # ENABLE_VSIM_GUI == 1
endif # VSIM not set

ifeq ($(SPD),)
ifneq ($(ABS_QUARTUS_PROJECT_DIR),)
ifneq ($(SOPC_NAME),)
SPD := $(ABS_QUARTUS_PROJECT_DIR)/$(SOPC_NAME)_tb.spd
endif # SOPC_NAME set
endif # ABS_QUARTUS_PROJECT_DIR set
endif # SPD == empty string

ifeq ($(MSIM_SCRIPT),)
SIM_SCRIPT_DIR := $(RUNTIME_ROOT_DIR)/sim
MSIM_SCRIPT := $(SIM_SCRIPT_DIR)/mentor/msim_setup.tcl
endif # MSIM_SCRIPT == empty string

ifeq ($(MAKE_VERSION),3.81)
ABS_MEM_INIT_DESCRIPTOR_FILE := $(abspath $(MEM_INIT_DESCRIPTOR_FILE))
else
ABS_MEM_INIT_DESCRIPTOR_FILE := $(call adjust-path-mixed,$(shell pwd))/$(MEM_INIT_DESCRIPTOR_FILE)
endif

$(MSIM_SCRIPT): $(SPD) $(MEM_INIT_DESCRIPTOR_FILE)
ifeq ($(SPD),)
	$(error No SPD file specified. Ensure QUARTUS_PROJECT_DIR variable is set)
endif
	@$(MKDIR) $(SIM_SCRIPT_DIR)
	$(IP_MAKE_SIMSCRIPT) --spd=$(SPD) --spd=$(MEM_INIT_DESCRIPTOR_FILE) --output-directory=$(SIM_SCRIPT_DIR)

VSIM_COMMAND = \
	cd $(dir $(MSIM_SCRIPT)) && \
	$(VSIM) -do "do $(notdir $(MSIM_SCRIPT)); ld; $(if $(VSIM_RUN_TIME),run ${VSIM_RUN_TIME};quit;)"

.PHONY: sim
sim: $(MSIM_SCRIPT) mem_init_generate
ifeq ($(MSIM_SCRIPT),)
	$(error MSIM_SCRIPT not set)
endif
	$(VSIM_COMMAND)

endif # QSYS == 1


#------------------------------------------------------------------------------
#                         ELF TARGET RULE
#------------------------------------------------------------------------------
# Rule for constructing the executable elf file.
$(ELF) : $(APP_OBJS) $(LINKER_SCRIPT) $(APP_LDDEPS)
	@$(ECHO) Info: Linking $@
	$(LD) $(APP_LDFLAGS) $(APP_CFLAGS) -o $@ $(filter-out $(CRT0),$(APP_OBJS)) $(APP_LIBS) $(APP_BSP_DEP_LIBS)
ifneq ($(DISABLE_ELFPATCH),1)
	$(ELFPATCH) $@ $(ELF_PATCH_FLAG)
endif
ifneq ($(DISABLE_STACKREPORT),1)
	@bash -c "$(STACKREPORT) $@"
endif

$(OBJDUMP_NAME) : $(ELF)
	@$(ECHO) Info: Creating $@
	$(OBJDUMP) $(OBJDUMP_FLAGS) $< >$@

# Rule for printing the name of the elf file
.PHONY: print-elf-name
print-elf-name:
	@$(ECHO) $(ELF)
