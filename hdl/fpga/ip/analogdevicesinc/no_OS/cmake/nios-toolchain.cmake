################################################################################
# Configure CMake and setup variables for the Nios II environment
#
# Inputs:
#   QUARTUS_ROOTDIR   Path to Quartus Prime installation.
#
#
################################################################################
cmake_minimum_required(VERSION 2.8)

if(NOT "$ENV{QUARTUS_ROOTDIR}" STREQUAL "")
    set(QUARTUS_ROOTDIR "$ENV{QUARTUS_ROOTDIR}")
endif()

# There seems to be some known strange behavior where CMake loses track of
# cached variables when (re)parsing toolchain files. This is a workaround
# identified in http://stackoverflow.com/questions/28613394/check-cmake-cache-variable-in-toolchain-file
if(QUARTUS_ROOTDIR)
    # Use the environment to back up this definition, per that above workaround
    set(ENV{_QUARTUS_ROOTDIR} "${QUARTUS_ROOTDIR}")
else()
    if(NOT "$ENV{_QUARTUS_ROOTDIR}" STREQUAL "")
        # Use the definition we backed up in the environment.
        set(QUARTUS_ROOTDIR "$ENV{_QUARTUS_ROOTDIR}")
    endif()
endif()

if("${QUARTUS_ROOTDIR}" STREQUAL "")
    message(FATAL_ERROR "QUARTUS_ROOTDIR not defined")
else()
    message(STATUS "QUARTUS_ROOTDIR = ${QUARTUS_ROOTDIR}")
endif()


################################################################################
# CMake toolchain file configuration
################################################################################

find_program(CMAKE_C_COMPILER nios2-elf-gcc ${QUARTUS_ROOTDIR}/nios2eds/)
find_program(CMAKE_CXX_COMPILER nios2-elf-g++ ${QUARTUS_ROOTDIR}/nios2eds/)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR nios2)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
