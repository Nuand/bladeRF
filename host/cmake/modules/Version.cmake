# Portions of this file have been borrowed from and/or inspired by
# the Version.cmake from the rtl-sdr project.
#   http://sdr.osmocom.org/trac/wiki/rtl-sdr
#
# Provides:
#   ${VERSION_INFO_BASE}     -  Major.Minor.Patch
#   ${VERSION_INFO}          -  Major.minor.Patch[-git_info]
#
# Requires values for:
#   ${VERSION_INFO_MAJOR}    - Increment on API compatibility changes.
#   ${VERSION_INFO_MINOR}    - Increment when adding features.
#   ${VERSION_INFO_PATCH}    - Increment for bug and documentation changes.
#
# Optional:
#   ${VERSION_INFO_EXTRA}    - Set to "git" to append git info. This is
#                              intended only for non-versioned development
#                              builds
#   ${VERSION_INFO_OVERRIDE} - Set to a non-null value to override the
#                              VERSION_INFO_EXTRA logic. This is intended
#                              for automated snapshot builds from exported
#                              trees, to pass in the git revision info.
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
        COMMAND ${GIT_EXECUTABLE} rev-parse --
        ERROR_QUIET
        RESULT_VARIABLE NOT_GIT_REPOSITORY
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    if(NOT_GIT_REPOSITORY)
        set(GIT_INFO "-unknown")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD --
            OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
            RESULT_VARIABLE GIT_DIRTY
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        if(GIT_DIRTY)
            set(GIT_INFO "-${GIT_REV}-dirty")
        else()
            set(GIT_INFO "-${GIT_REV}")
        endif()
    endif()

else()
    message(WARNING "git missing -- unable to check libladeRF version.")
    unset(NOT_GIT_REPOSITORY)
    unset(GIT_REV)
    unset(GIT_DIRTY)
endif()


################################################################################
# Provide 
################################################################################
set(VERSION_INFO_BASE "${VER_MAJ}.${VER_MIN}.${VER_PAT}")

# Force the version suffix.  Used for automated export builds.
if(VERSION_INFO_OVERRIDE)
    set(VERSION_INFO "${VERSION_INFO_BASE}-${VERSION_INFO_OVERRIDE}")

# Intra-release builds
elseif("${VERSION_INFO_EXTRA}" STREQUAL "git")
    set(VERSION_INFO "${VERSION_INFO_BASE}-git${GIT_INFO}")

# Versioned releases
elseif("${VERSION_INFO_EXTRA}" STREQUAL "")
    set(VERSION_INFO "${VERSION_INFO_BASE}")

# Invalid 
else() 
    message(FATAL_ERROR 
        "Unexpected definition of VERSION_INFO_EXTRA: ${VERSION_INFO_EXTRA}")
endif()
