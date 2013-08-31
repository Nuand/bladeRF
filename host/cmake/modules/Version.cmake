# Copyright 2012 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
#
# Portions of this file have been borrowed from and/or inspired by
# the Version.cmake from the rtl-sdr project.
#   http://sdr.osmocom.org/trac/wiki/rtl-sdr
#
#
#
# Provides:
#   ${VERSION_INFO_BASE}  -  Major.Minor.Patch
#   ${VERSION_INFO}       -  Major.minor.Patch[-git_info] 
#
# Requires values for:
#   ${VERSION_INFO_MAJOR} - Increment on API compatibility changes.
#   ${VERSION_INFO_MINOR} - Increment when adding features.
#   ${VERSION_INFO_PATCH} - Increment for bug and documenation changes.
#
# Optional:
#   ${VERSION_INFO_EXTRA} - Set to "git" to append git info. This is intended
#                           only non-versioned development builds
#
if(DEFINED __INCLUDED_BLADERF_VERSION_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_VERSION_CMAKE TRUE)

################################################################################
# Gather up variables provided by parent script
################################################################################

if(NOT DEFINED VERSION_INFO_MAJOR)
    message(FATAL_ERROR "VERSION_INFO_MAJOR is not defined")
else()            
    set(VER_MAJ ${VERSION_INFO_MAJOR})
endif()

if(NOT DEFINED VERSION_INFO_MINOR)
    message(FATAL_ERROR "VERSION_INFO_MINOR is not defined")
else()            
    set(VER_MIN ${VERSION_INFO_MINOR})
endif()

if(NOT DEFINED VERSION_INFO_PATCH)
    message(FATAL_ERROR "VERSION_INFO_PATCH is not defined")
else()            
    set(VER_PAT ${VERSION_INFO_PATCH})
endif()


################################################################################
# Craft version number, using git, if needed
################################################################################
find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD -- 
        OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD -- 
        RESULT_VARIABLE GIT_NOT_DIRTY
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    if(GET_NOT_DIRTY)
        set(GIT_STATE "")
    else()
        set(GIT_STATE "-dirty")
    endif()

else()
    message(WARNING "git missing -- unable to check libladeRF version.")
    set(GIT_CHANGESET "unknown")   
    set(GIT_DIRTY "")
endif()


################################################################################
# Provide 
################################################################################
set(VERSION_INFO_BASE "${VER_MAJ}.${VER_MIN}.${VER_PAT}")

# Intra-release builds
if("${VERSION_INFO_EXTRA}" STREQUAL "git")
    set(VERSION_INFO "${VERSION_INFO_BASE}-git-${GIT_REV}${GIT_STATE}")

# Versioned releases
elseif("${VERSION_INFO_EXTRA}" STREQUAL "")
    set(VERSION_INFO "${VERSION_INFO_BASE}")

# Invalid 
else() 
    message(FATAL_ERROR 
        "Unexpected definition of VERSION_INFO_EXTRA: ${VERSION_INFO_EXTRA}")
endif()
