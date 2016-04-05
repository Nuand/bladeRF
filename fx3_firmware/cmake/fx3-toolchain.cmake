################################################################################
# Configure CMake and setup variables for the Cypress FX3 SDK v1.3.3
#
# Inputs:
#   FX3_INSTALL_PATH    Path to Cypress FX3 SDK Installation.
#                       This variable name coincides with the environment
#                       variable added by the FX3 SDK installer
#
#
################################################################################
cmake_minimum_required(VERSION 2.8)

################################################################################
# FX3 SDK paths and files
################################################################################

set(FX3_SDK_VERSION         "1.3.3")     
set(ARM_TOOLCHAIN_VERSION   "2013.11")
set(GCC_VERSION             "4.8.1")     

# There seems to be some known strange behavior where CMake loses track of
# cached variables when (re)parsing toolchain files. This is a workaround
# identified in http://stackoverflow.com/questions/28613394/check-cmake-cache-variable-in-toolchain-file
if(FX3_INSTALL_PATH)
    # Use the environment to back up this definition, per that above workaround
    set(ENV{_FX3_INSTALL_PATH} "${FX3_INSTALL_PATH}")
else()
    if(NOT "$ENV{_FX3_INSTALL_PATH}" STREQUAL "")
        # Use the definition we backed up in the environment.
        set(FX3_INSTALL_PATH "$ENV{_FX3_INSTALL_PATH}")
    else()
        # Try falling back to a path definition (created by Windows installer)
        set(FX3_INSTALL_PATH "$ENV{FX3_INSTALL_PATH}")
    endif()
endif()

# Paths to a specfic version use underscores instead of paths
string(REPLACE "." "_" FX3_SDK_VERSION_PATH "${FX3_SDK_VERSION}")

if(WIN32)
    if("${FX3_INSTALL_PATH}" STREQUAL "")
        set(FX3_INSTALL_PATH "C:/Program Files (x86)/Cypress/EZ-USB FX3 SDK/1.3")
        message("FX3_INSTALL_PATH not specified. Falling back to default.")
    endif()

    set(FX3_WINDOWS_HOST True)
    set(EXE ".exe")

    set(ARM_NONE_EABI_PATH "${FX3_INSTALL_PATH}/ARM GCC")
    set(FX3_FWLIB_DIR "${FX3_INSTALL_PATH}/fw_lib/${FX3_SDK_VERSION_PATH}")
    set(FX3_FW_COMMON_DIR "${FX3_INSTALL_PATH}/firmware/common")
    set(FX3_ELF2IMG "${FX3_INSTALL_PATH}/util/elf2img/elf2img.exe")
    set(CMAKE_MAKE_PROGRAM "${FX3_INSTALL_PATH}/ARM GCC/bin/cs-make.exe")

else()
    if("${FX3_INSTALL_PATH}" STREQUAL "")
        set(FX3_INSTALL_PATH "/opt/cypress/fx3_sdk")
        message("FX3_INSTALL_PATH not specified. Falling back to default.")
    endif()

    set(FX3_WINDOWS_HOST False)
    unset(EXE)

    set(ARM_NONE_EABI_PATH  "${FX3_INSTALL_PATH}/arm-${ARM_TOOLCHAIN_VERSION}")
    set(FX3_FWLIB_DIR "${FX3_INSTALL_PATH}/cyfx3sdk/fw_lib/${FX3_SDK_VERSION_PATH}")
    set(FX3_FW_COMMON_DIR "${FX3_INSTALL_PATH}/cyfx3sdk/firmware/common")
    set(FX3_ELF2IMG "${FX3_INSTALL_PATH}/cyfx3sdk/util/elf2img/elf2img.c")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(FX3_LIBRARY_DIR "${FX3_FWLIB_DIR}/fx3_debug")
else()
    set(FX3_LIBRARY_DIR "${FX3_FWLIB_DIR}/fx3_release")
endif()

set(FX3_WINDOWS_HOST ${FX3_WINDOWS_HOST}                CACHE PATH "Cross-build host is Windows")
set(FX3_INSTALL_PATH ${FX3_INSTALL_PATH}                CACHE PATH "Path to FX3 SDK")
set(FX3_INCLUDE_DIR "${FX3_FWLIB_DIR}/inc"              CACHE PATH "Path to FX3 header directory")
set(FX3_LIBRARY_DIR "${FX3_LIBRARY_DIR}"                CACHE PATH "FX3 Library directory")
set(FX3_FW_COMMON_DIR "${FX3_FW_COMMON_DIR}"            CACHE PATH "FX3 SDK directory containing common linker and startup source files")
set(FX3_LINKER_FILE "${FX3_FW_COMMON_DIR}/fx3_512k.ld"  CACHE PATH "FX3 Linker script")
set(FX3_ELF2IMG "${FX3_ELF2IMG}"                        CACHE PATH "FX3 ELF to boot image converter source (Linux) or executable (Windows)")

# Perform some sanity checks on the above paths
if(NOT EXISTS "${FX3_LINKER_FILE}")
    message(FATAL_ERROR "FX3 SDK: Could not find linker file: ${FX3_LINKER_FILE}")
endif()

if(NOT EXISTS "${FX3_FWLIB_DIR}")
    message(FATAL_ERROR "FX3 SDK: Could not find ${FX3_FWLIB_DIR}")
endif()

if(NOT EXISTS "${FX3_INCLUDE_DIR}/cyu3os.h")
    message(FATAL_ERROR "cyu3os.h is missing. Check FX3_INCLUDE_DIR definition.")
endif()

if(NOT EXISTS "${FX3_LIBRARY_DIR}/libcyfxapi.a")
    message(FATAL_ERROR "libcyfxapi.a is missing. Check FX3_LIBRARY_DIR definition.")
endif()

if(NOT EXISTS "${FX3_ELF2IMG}")
    message(FATAL_ERROR "${FX3_ELF2IMG} is missing.")
endif()

################################################################################
# ARM Toolchain paths and sanity checks
################################################################################

set(CMAKE_C_COMPILER "${ARM_NONE_EABI_PATH}/bin/arm-none-eabi-gcc${EXE}")

set(ARM_NONE_EABI_LIBGCC
    "${ARM_NONE_EABI_PATH}/lib/gcc/arm-none-eabi/${GCC_VERSION}/libgcc.a"
    CACHE PATH "Path to FX3 SDK's libgcc.a")

set(ARM_NONE_EABI_LIBC "${ARM_NONE_EABI_PATH}/arm-none-eabi/lib/libc.a"
    CACHE PATH "Path to FX3 SDK's libc.a")

if(NOT EXISTS ${CMAKE_C_COMPILER})
    message(FATAL_ERROR "Could not find compiler: ${CMAKE_C_COMPILER}")
endif()

if(NOT EXISTS ${ARM_NONE_EABI_LIBGCC})
    message(FATAL_ERROR "Could not find libgcc.a")
endif()

if(NOT EXISTS ${ARM_NONE_EABI_LIBC})
    message(FATAL_ERROR "Could not find libc.a")
endif()


################################################################################
# CMake toolchain file configuration
################################################################################

set(CMAKE_SYSTEM_NAME Generic)

# Search for programms in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH ${ARM_NONE_EABI_PATH})
