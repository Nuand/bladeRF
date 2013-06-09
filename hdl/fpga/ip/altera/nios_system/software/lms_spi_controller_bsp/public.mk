#------------------------------------------------------------------------------
#                       BSP "PUBLIC" MAKEFILE CONTENT
#
# This file is intended to be included in an application or library
# Makefile that is using this BSP. You can create such a Makefile with
# the nios2-app-generate-makefile or nios2-lib-generate-makefile
# commands.
#
# The following variables must be defined before including this file:
#
#     ALT_LIBRARY_ROOT_DIR
#         Contains the path to the BSP top-level (aka root) directory
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
#                                  PATHS
#------------------------------------------------------------------------------



# Path to the provided linker script.
BSP_LINKER_SCRIPT := $(ALT_LIBRARY_ROOT_DIR)/linker.x

# Include paths:
# The path to root of all header files that a library wishes to make
# available for an application's use is specified here. Note that this
# may not be *all* folders within a hierarchy. For example, if it is
# desired that the application developer type:
#   #include <sockets.h>
#   #include <ip/tcpip.h>
# With files laid out like this:
#   <root of library>/inc/sockets.h
#   <root of library>/inc/ip/tcpip.h
#
# Then, only <root of library>/inc need be added to the list of include
# directories. Alternatively, if you wish to be able to directly include
# all files in a hierarchy, separate paths to each folder in that
# hierarchy must be defined.

# The following are the "base" set of include paths for a BSP.
# These paths are appended to the list that individual software
# components, drivers, etc., add in the generated portion of this
# file (below).
ALT_INCLUDE_DIRS_TO_APPEND += \
        $(ALT_LIBRARY_ROOT_DIR) \
        $(ALT_LIBRARY_ROOT_DIR)/drivers/inc

# Additions to linker library search-path:
# Here we provide a path to "our self" for the application to construct a
# "-L <path to BSP>" out of. This should contain a list of directories,
# relative to the library root, of all directories with .a files to link
# against.
ALT_LIBRARY_DIRS += $(ALT_LIBRARY_ROOT_DIR)


#------------------------------------------------------------------------------
#                               COMPILATION FLAGS
#------------------------------------------------------------------------------
# Default C pre-processor flags for a BSP:
ALT_CPPFLAGS += -DSYSTEM_BUS_WIDTH=32 \
                -pipe


#------------------------------------------------------------------------------
#                              MANAGED CONTENT
#
# All content between the lines "START MANAGED" and "END MANAGED" below is
# generated based on variables in the BSP settings file when the
# nios2-bsp-generate-files command is invoked. If you wish to persist any
# information pertaining to the build process, it is recomended that you
# utilize the BSP settings mechanism to do so.
#------------------------------------------------------------------------------
#START MANAGED

# The following TYPE comment allows tools to identify the 'type' of target this 
# makefile is associated with. 
# TYPE: BSP_PUBLIC_MAKEFILE

# This following VERSION comment indicates the version of the tool used to 
# generate this makefile. A makefile variable is provided for VERSION as well. 
# ACDS_VERSION: 12.1sp1
ACDS_VERSION := 12.1sp1

# This following BUILD_NUMBER comment indicates the build number of the tool 
# used to generate this makefile. 
# BUILD_NUMBER: 243

# Qsys--generated SOPCINFO file. Required for resolving node instance ID's with 
# design component names. 
SOPCINFO_FILE := $(ABS_BSP_ROOT_DIR)/../../nios_system.sopcinfo

# Big-Endian operation. 
# setting BIG_ENDIAN is false
ALT_CFLAGS += -EL

# Path to the provided C language runtime initialization code. 
BSP_CRT0 := $(ALT_LIBRARY_ROOT_DIR)/obj/HAL/src/crt0.o

# Name of BSP library as provided to linker using the "-msys-lib" flag or 
# linker script GROUP command. 
# setting BSP_SYS_LIB is hal_bsp
BSP_SYS_LIB := hal_bsp
ELF_PATCH_FLAG  += --thread_model hal

# Type identifier of the BSP library 
# setting BSP_TYPE is hal
ALT_CPPFLAGS += -D__hal__
BSP_TYPE := hal

# CPU Name 
# setting CPU_NAME is nios2_qsys_0
CPU_NAME = nios2_qsys_0
ELF_PATCH_FLAG  += --cpu_name $(CPU_NAME)

# Hardware Divider present. 
# setting HARDWARE_DIVIDE is false
ALT_CFLAGS += -mno-hw-div

# Hardware Multiplier present. 
# setting HARDWARE_MULTIPLY is false
ALT_CFLAGS += -mno-hw-mul

# Hardware Mulx present. 
# setting HARDWARE_MULX is false
ALT_CFLAGS += -mno-hw-mulx

# Debug Core present. 
# setting HAS_DEBUG_CORE is true
CPU_HAS_DEBUG_CORE = 1

# Qsys generated design 
# setting QSYS is 1
QSYS := 1
ELF_PATCH_FLAG += --qsys true

# Design Name 
# setting SOPC_NAME is nios_system
SOPC_NAME := nios_system

# SopcBuilder Simulation Enabled 
# setting SOPC_SIMULATION_ENABLED is false
ELF_PATCH_FLAG  += --simulation_enabled false

# Small-footprint (polled mode) driver none 
# setting altera_avalon_jtag_uart_driver.enable_small_driver is false

# Enable driver ioctl() support. This feature is not compatible with the 
# 'small' driver; ioctl() support will not be compiled if either the UART 
# 'enable_small_driver' or HAL 'enable_reduced_device_drivers' settings are 
# enabled. none 
# setting altera_avalon_uart_driver.enable_ioctl is false

# Small-footprint (polled mode) driver none 
# setting altera_avalon_uart_driver.enable_small_driver is false

# Build a custom version of newlib with the specified space-separated compiler 
# flags. The custom newlib build will be placed in the &lt;bsp root>/newlib 
# directory, and will be used only for applications that utilize this BSP. 
# setting hal.custom_newlib_flags is none

# Enable support for a subset of the C++ language. This option increases code 
# footprint by adding support for C++ constructors. Certain features, such as 
# multiple inheritance and exceptions are not supported. If false, adds 
# -DALT_NO_C_PLUS_PLUS to ALT_CPPFLAGS in public.mk, and reduces code 
# footprint. none 
# setting hal.enable_c_plus_plus is 0
ALT_CPPFLAGS += -DALT_NO_C_PLUS_PLUS

# When your application exits, close file descriptors, call C++ destructors, 
# etc. Code footprint can be reduced by disabling clean exit. If disabled, adds 
# -DALT_NO_CLEAN_EXIT to ALT_CPPFLAGS and -Wl,--defsym, exit=_exit to 
# ALT_LDFLAGS in public.mk. none 
# setting hal.enable_clean_exit is 0
ALT_CPPFLAGS += -DALT_NO_CLEAN_EXIT
ALT_LDFLAGS += -Wl,--defsym,exit=_exit

# Add exit() support. This option increases code footprint if your "main()" 
# routine does "return" or call "exit()". If false, adds -DALT_NO_EXIT to 
# ALT_CPPFLAGS in public.mk, and reduces footprint none 
# setting hal.enable_exit is 0
ALT_CPPFLAGS += -DALT_NO_EXIT

# Causes code to be compiled with gprof profiling enabled and the application 
# ELF to be linked with the GPROF library. If true, adds -DALT_PROVIDE_GMON to 
# ALT_CPPFLAGS and -pg to ALT_CFLAGS in public.mk. none 
# setting hal.enable_gprof is 0

# Enables lightweight device driver API. This reduces code and data footprint 
# by removing the HAL layer that maps device names (e.g. /dev/uart0) to file 
# descriptors. Instead, driver routines are called directly. The open(), 
# close(), and lseek() routines will always fail if called. The read(), 
# write(), fstat(), ioctl(), and isatty() routines only work for the stdio 
# devices. If true, adds -DALT_USE_DIRECT_DRIVERS to ALT_CPPFLAGS in public.mk. 
# The Altera Host and read-only ZIP file systems can't be used if 
# hal.enable_lightweight_device_driver_api is true. 
# setting hal.enable_lightweight_device_driver_api is 1
ALT_CPPFLAGS += -DALT_USE_DIRECT_DRIVERS

# Adds code to emulate multiply and divide instructions in case they are 
# executed but aren't present in the CPU. Normally this isn't required because 
# the compiler won't use multiply and divide instructions that aren't present 
# in the CPU. If false, adds -DALT_NO_INSTRUCTION_EMULATION to ALT_CPPFLAGS in 
# public.mk. none 
# setting hal.enable_mul_div_emulation is 0
ALT_CPPFLAGS += -DALT_NO_INSTRUCTION_EMULATION

# Certain drivers are compiled with reduced functionality to reduce code 
# footprint. Not all drivers observe this setting. The altera_avalon_uart and 
# altera_avalon_jtag_uart drivers switch from interrupt-driven to polled 
# operation. CAUTION: Several device drivers are disabled entirely. These 
# include the altera_avalon_cfi_flash, altera_avalon_epcs_flash_controller, and 
# altera_avalon_lcd_16207 drivers. This can result in certain API (HAL flash 
# access routines) to fail. You can define a symbol provided by each driver to 
# prevent it from being removed. If true, adds -DALT_USE_SMALL_DRIVERS to 
# ALT_CPPFLAGS in public.mk. none 
# setting hal.enable_reduced_device_drivers is 1
ALT_CPPFLAGS += -DALT_USE_SMALL_DRIVERS

# Turns on HAL runtime stack checking feature. Enabling this setting causes 
# additional code to be placed into each subroutine call to generate an 
# exception if a stack collision occurs with the heap or statically allocated 
# data. If true, adds -DALT_STACK_CHECK and -mstack-check to ALT_CPPFLAGS in 
# public.mk. none 
# setting hal.enable_runtime_stack_checking is 0

# The BSP is compiled with optimizations to speedup HDL simulation such as 
# initializing the cache, clearing the .bss section, and skipping long delay 
# loops. If true, adds -DALT_SIM_OPTIMIZE to ALT_CPPFLAGS in public.mk. When 
# this setting is true, the BSP shouldn't be used to build applications that 
# are expected to run real hardware. 
# setting hal.enable_sim_optimize is 0

# Causes the small newlib (C library) to be used. This reduces code and data 
# footprint at the expense of reduced functionality. Several newlib features 
# are removed such as floating-point support in printf(), stdin input routines, 
# and buffered I/O. The small C library is not compatible with Micrium 
# MicroC/OS-II. If true, adds -msmallc to ALT_LDFLAGS in public.mk. none 
# setting hal.enable_small_c_library is 1
ALT_LDFLAGS += -msmallc
ALT_CPPFLAGS += -DSMALL_C_LIB

# Enable SOPC Builder System ID. If a System ID SOPC Builder component is 
# connected to the CPU associated with this BSP, it will be enabled in the 
# creation of command-line arguments to download an ELF to the target. 
# Otherwise, system ID and timestamp values are left out of public.mk for 
# application Makefile "download-elf" target definition. With the system ID 
# check disabled, the Nios II EDS tools will not automatically ensure that the 
# application .elf file (and BSP it is linked against) corresponds to the 
# hardware design on the target. If false, adds --accept-bad-sysid to 
# SOPC_SYSID_FLAG in public.mk. none 
# setting hal.enable_sopc_sysid_check is 1

# Enable BSP generation to query if SOPC system is big endian. If true ignores 
# export of 'ALT_CFLAGS += -EB' to public.mk if big endian system. If true 
# ignores export of 'ALT_CFLAGS += -EL' if little endian system. none 
# setting hal.make.ignore_system_derived.big_endian is 0

# Enable BSP generation to query if SOPC system has a debug core present. If 
# true ignores export of 'CPU_HAS_DEBUG_CORE = 1' to public.mk if a debug core 
# is found in the system. If true ignores export of 'CPU_HAS_DEBUG_CORE = 0' if 
# no debug core is found in the system. none 
# setting hal.make.ignore_system_derived.debug_core_present is 0

# Enable BSP generation to query if SOPC system has FPU present. If true 
# ignores export of 'ALT_CFLAGS += -mhard-float' to public.mk if FPU is found 
# in the system. If true ignores export of 'ALT_CFLAGS += -mhard-soft' if FPU 
# is not found in the system. none 
# setting hal.make.ignore_system_derived.fpu_present is 0

# Enable BSP generation to query if SOPC system has hardware divide present. If 
# true ignores export of 'ALT_CFLAGS += -mno-hw-div' to public.mk if no 
# division is found in system. If true ignores export of 'ALT_CFLAGS += 
# -mhw-div' if division is found in the system. none 
# setting hal.make.ignore_system_derived.hardware_divide_present is 0

# Enable BSP generation to query if SOPC system floating point custom 
# instruction with a divider is present. If true ignores export of 'ALT_CFLAGS 
# += -mcustom-fpu-cfg=60-2' and 'ALT_LDFLAGS += -mcustom-fpu-cfg=60-2' to 
# public.mk if the custom instruction is found in the system. none 
# setting hal.make.ignore_system_derived.hardware_fp_cust_inst_divider_present is 0

# Enable BSP generation to query if SOPC system floating point custom 
# instruction without a divider is present. If true ignores export of 
# 'ALT_CFLAGS += -mcustom-fpu-cfg=60-1' and 'ALT_LDFLAGS += 
# -mcustom-fpu-cfg=60-1' to public.mk if the custom instruction is found in the 
# system. none 
# setting hal.make.ignore_system_derived.hardware_fp_cust_inst_no_divider_present is 0

# Enable BSP generation to query if SOPC system has multiplier present. If true 
# ignores export of 'ALT_CFLAGS += -mno-hw-mul' to public.mk if no multiplier 
# is found in the system. If true ignores export of 'ALT_CFLAGS += -mhw-mul' if 
# multiplier is found in the system. none 
# setting hal.make.ignore_system_derived.hardware_multiplier_present is 0

# Enable BSP generation to query if SOPC system has hardware mulx present. If 
# true ignores export of 'ALT_CFLAGS += -mno-hw-mulx' to public.mk if no mulx 
# is found in the system. If true ignores export of 'ALT_CFLAGS += -mhw-mulx' 
# if mulx is found in the system. none 
# setting hal.make.ignore_system_derived.hardware_mulx_present is 0

# Enable BSP generation to query if SOPC system has simulation enabled. If true 
# ignores export of 'ELF_PATCH_FLAG += --simulation_enabled' to public.mk. none 
# setting hal.make.ignore_system_derived.sopc_simulation_enabled is 0

# Enable BSP generation to query SOPC system for system ID base address. If 
# true ignores export of 'SOPC_SYSID_FLAG += --sidp=<address>' and 
# 'ELF_PATCH_FLAG += --sidp=<address>' to public.mk. none 
# setting hal.make.ignore_system_derived.sopc_system_base_address is 0

# Enable BSP generation to query SOPC system for system ID. If true ignores 
# export of 'SOPC_SYSID_FLAG += --id=<sysid>' and 'ELF_PATCH_FLAG += 
# --id=<sysid>' to public.mk. none 
# setting hal.make.ignore_system_derived.sopc_system_id is 0

# Enable BSP generation to query SOPC system for system timestamp. If true 
# ignores export of 'SOPC_SYSID_FLAG += --timestamp=<timestamp>' and 
# 'ELF_PATCH_FLAG += --timestamp=<timestamp>' to public.mk. none 
# setting hal.make.ignore_system_derived.sopc_system_timestamp is 0

# Slave descriptor of STDERR character-mode device. This setting is used by the 
# ALT_STDERR family of defines in system.h. none 
# setting hal.stderr is jtag_uart_0
ELF_PATCH_FLAG  += --stderr_dev jtag_uart_0

# Slave descriptor of STDIN character-mode device. This setting is used by the 
# ALT_STDIN family of defines in system.h. none 
# setting hal.stdin is jtag_uart_0
ELF_PATCH_FLAG  += --stdin_dev jtag_uart_0

# Slave descriptor of STDOUT character-mode device. This setting is used by the 
# ALT_STDOUT family of defines in system.h. none 
# setting hal.stdout is jtag_uart_0
ELF_PATCH_FLAG  += --stdout_dev jtag_uart_0


#------------------------------------------------------------------------------
#                 SOFTWARE COMPONENT & DRIVER INCLUDE PATHS
#------------------------------------------------------------------------------

ALT_INCLUDE_DIRS += $(ALT_LIBRARY_ROOT_DIR)/HAL/inc

#------------------------------------------------------------------------------
#        SOFTWARE COMPONENT & DRIVER PRODUCED ALT_CPPFLAGS ADDITIONS
#------------------------------------------------------------------------------

ALT_CPPFLAGS += -DALT_SINGLE_THREADED

#END MANAGED


#------------------------------------------------------------------------------
#                             LIBRARY INFORMATION
#------------------------------------------------------------------------------
# Assemble the name of the BSP *.a file using the BSP library name
# (BSP_SYS_LIB) in generated content above.
BSP_LIB := lib$(BSP_SYS_LIB).a

# Additional libraries to link against:
# An application including this file will prefix each library with "-l".
# For example, to include the Newlib math library "m" is included, which
# becomes "-lm" when linking the application.
ALT_LIBRARY_NAMES += m

# Additions to linker dependencies:
# An application Makefile will typically add these directly to the list 
# of dependencies required to build the executable target(s). The BSP
# library (*.a) file is specified here.
ALT_LDDEPS += $(ALT_LIBRARY_ROOT_DIR)/$(BSP_LIB)

# Is this library "Makeable"?
# Add to list of root library directories that support running 'make'
# to build them. Because libraries may or may not have a Makefile in their
# root, appending to this variable tells an application to run 'make' in
# the library root to build/update this library.
MAKEABLE_LIBRARY_ROOT_DIRS += $(ALT_LIBRARY_ROOT_DIR)

# Additional Assembler Flags
# -gdwarf2 flag is required for stepping through assembly code
ALT_ASFLAGS += -gdwarf2

#------------------------------------------------------------------------------
#                       FINAL INCLUDE PATH LIST
#------------------------------------------------------------------------------
# Append static include paths to paths specified by OS/driver/sw package 
# additions to the BSP thus giving them precedence in case a BSP addition
# is attempting to override BSP sources.
ALT_INCLUDE_DIRS += $(ALT_INCLUDE_DIRS_TO_APPEND)



