# Define a SHORT_FILE_ macro on a per-file basis to yield
# something akin to __FILE__, but with only the file name
#
# Parameters:
#
# SRC_TO_SHORTEN -   List of source files to create short names for
foreach(FILE ${SRC_TO_SHORTEN})
    get_filename_component(FILE_NAME ${FILE} NAME)
    set_source_files_properties(${FILE} PROPERTIES
                                COMPILE_FLAGS -DSHORT_FILE_=\"${FILE_NAME}\")
endforeach()
