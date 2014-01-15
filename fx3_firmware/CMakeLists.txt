cmake_minimum_required(VERSION 2.8.3)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../host/cmake/modules)
set(VERSION_INFO_MAJOR 1)
set(VERSION_INFO_MINOR 6)
set(VERSION_INFO_PATCH 1)
if(NOT DEFINED VERSION_INFO_EXTRA)
    set(VERSION_INFO_EXTRA "git")
endif()
include(Version)

set(VERSION "${VERSION_INFO}")

# .dirstamp needs to touched before version.h so that fx3_firmware/makefile
# doesn't try to rebuild version.h.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/build/.dirstamp
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/build/version.h
    @ONLY
)

add_custom_target(fx3_firmware ALL
    COMMAND ${CMAKE_MAKE_PROGRAM} OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/build)

