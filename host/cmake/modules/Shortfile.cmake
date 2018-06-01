if(DEFINED __INCLUDED_BLADERF_SHORTFILE_CMAKE)
    return()
endif()
set(__INCLUDED_BLADERF_SHORTFILE_CMAKE TRUE)

# Based on:
# https://stackoverflow.com/questions/1706346/file-macro-manipulation-handling-at-compile-time/27990434#27990434

function(define_file_basename_for_sources targetname)
    # Trim /host from the end of CMAKE_HOME_DIRECTORY if required
    string(REGEX REPLACE "/host$" "" rootpath "${CMAKE_HOME_DIRECTORY}")

    # Get the list of files to set the SHORT_FILE_ definition for
    get_target_property(source_files "${targetname}" SOURCES)

    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)

        # Resolve the filename to an absolute pathname with symlinks resolved
        get_filename_component(absname "${sourcefile}" REALPATH)

        # Trim the rootpath from the filename
        string(REPLACE "${rootpath}/" "" basename "${absname}")

        # Add the SHORT_FILE_=filename compile definition to the list.
        list(APPEND defs "SHORT_FILE_=${basename}")

        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs})
    endforeach()
endfunction()
