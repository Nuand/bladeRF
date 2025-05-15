cmake_minimum_required(VERSION 3.10...3.27)

if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bladeRF-fsk/c)
    message(STATUS "bladeRF-fsk: Attempting to initialize missing submodule")

    find_package(Git QUIET)

    if(Git_FOUND)
        execute_process(
            COMMAND ${Git_EXECUTABLE} submodule init
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        execute_process(
            COMMAND ${Git_EXECUTABLE} submodule update
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()

if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bladeRF-fsk/c)
    message(FATAL_ERROR "Directory ${CMAKE_CURRENT_SOURCE_DIR}/bladeRF-fsk was not found. "
                        "It appears that this submodule has not been properly "
                        "initialized. Please run 'git submodule init' and "
                        "'git submodule update', and then re-run cmake.")
endif()

add_subdirectory(bladeRF-cli)
add_subdirectory(bladeRF-update)
if(NOT MSVC)
    add_subdirectory(bladeRF-power)
    add_subdirectory(bladeRF-fsk/c)
endif()
