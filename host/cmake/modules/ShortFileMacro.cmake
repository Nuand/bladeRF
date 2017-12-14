# Define a SHORT_FILE_ macro on a per-file basis to yield
# something akin to __FILE__, but with only the relative path
#
# We assume this file is located in a directory three levels deep from the
# root of the project, e.g. host/cmake/modules/.  If not, change the number
# of "/.." appearing in the PROJECT_ROOT_DIR setting below, or set it in
# CMakeLists.txt.
#
# Note: while CMAKE_HOME_DIRECTORY is a reasonable choice, it will vary
# depending on whether you build from the root, or from host/.
#
# Parameters:
#
# PROJECT_ROOT_DIR - Root directory of the full project (optional)
# SRC_TO_SHORTEN -   List of source files to create short names for

if(NOT PROJECT_ROOT_DIR)
    set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()

# Resolve PROJECT_ROOT_DIR into an absolute path
get_filename_component(BASEPATH "${PROJECT_ROOT_DIR}" REALPATH)

foreach(FILE ${SRC_TO_SHORTEN})
    get_filename_component(FULL_FILE_NAME ${FILE} REALPATH)
    string(REPLACE "${BASEPATH}/" "" FILE_NAME ${FULL_FILE_NAME})

    # We can trim off a few remaining filename chunks without ambiguity
    string(REGEX REPLACE "^host/libraries/" "" FILE_NAME ${FILE_NAME})
    string(REGEX REPLACE "^host/utilities/" "" FILE_NAME ${FILE_NAME})

    set_source_files_properties(${FILE} PROPERTIES
                                COMPILE_FLAGS -DSHORT_FILE_=\"${FILE_NAME}\")

    #message(STATUS "Shortening: ${FULL_FILE_NAME} -> ${FILE_NAME}")
endforeach()
