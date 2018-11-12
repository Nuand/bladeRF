if(DEFINED __INCLUDED_BLADERF_FIND_PATCHER)
    return()
endif()
set(__INCLUDED_BLADERF_FIND_PATCHER TRUE)

# Look for the packages we want
find_package(Patch QUIET)
find_package(Git QUIET)

# Work around cross-platform case sensitivity issues
if(PATCH_FOUND)
    set(Patch_FOUND ${PATCH_FOUND})
    if (PATCH_EXECUTABLE)
        set(Patch_EXECUTABLE ${PATCH_EXECUTABLE})
    endif()
endif()

if(GIT_FOUND)
    set(Git_FOUND ${GIT_FOUND})
    if (GIT_EXECUTABLE)
        set(Git_EXECUTABLE ${GIT_EXECUTABLE})
    endif()
endif()

# patch is preferred, but git-apply is also okay.
if(Patch_FOUND)
    set(Patcher_FOUND TRUE)
    set(Patcher_EXECUTABLE ${Patch_EXECUTABLE})
elseif(Git_FOUND)
    set(Patcher_FOUND TRUE)
    set(Patcher_EXECUTABLE ${Git_EXECUTABLE};--bare;apply;-)
else()
    set(Patcher_FOUND FALSE)
endif()

if(NOT Patcher_FOUND AND Patcher_FIND_REQUIRED)
    message(FATAL_ERROR "FindPatcher: Could not find either 'patch' or 'git'!")
endif()

if(Patcher_FOUND AND NOT Patcher_FIND_QUIETLY)
    message(STATUS "FindPatcher: found ${Patcher_EXECUTABLE}")
endif()
