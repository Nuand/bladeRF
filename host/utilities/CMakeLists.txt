cmake_minimum_required(VERSION 3.10...3.27)

option(BUILD_BLADERF_FSK "Build bladeRF FSK" ON)

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
    if(BUILD_BLADERF_FSK)
        add_subdirectory(bladeRF-fsk/c)
    endif()
endif()

################################################################################
# Shell completion installation
################################################################################
option(INSTALL_SHELL_COMPLETIONS "Install shell completions" ON)

if(INSTALL_SHELL_COMPLETIONS AND UNIX)
    # Set default paths if not specified
    if(NOT DEFINED BASH_COMPLETION_DIR)
        # List of common bash completion directories to check
        set(BASH_COMPLETION_SEARCH_DIRS
            "/opt/homebrew/share/bash-completion/completions"     # macOS ARM Homebrew
            "/usr/local/share/bash-completion/completions"        # macOS Intel Homebrew / FreeBSD
            "/usr/share/bash-completion/completions"              # Linux standard
            "/etc/bash_completion.d"                              # Legacy Linux
        )
        
        # Find the first existing directory
        foreach(dir ${BASH_COMPLETION_SEARCH_DIRS})
            if(EXISTS ${dir})
                set(BASH_COMPLETION_DIR ${dir} CACHE STRING "Directory for bash completion files")
                break()
            endif()
        endforeach()
    endif()
    
    if(NOT DEFINED ZSH_COMPLETION_DIR)
        # List of common zsh completion directories to check
        set(ZSH_COMPLETION_SEARCH_DIRS
            "/opt/homebrew/share/zsh/site-functions"              # macOS ARM Homebrew
            "/usr/local/share/zsh/site-functions"                 # macOS Intel Homebrew / FreeBSD
            "/usr/share/zsh/site-functions"                       # Linux standard
            "/usr/share/zsh/vendor-completions"                   # Some Linux distros
        )
        
        # Find the first existing directory
        foreach(dir ${ZSH_COMPLETION_SEARCH_DIRS})
            if(EXISTS ${dir})
                set(ZSH_COMPLETION_DIR ${dir} CACHE STRING "Directory for zsh completion files")
                break()
            endif()
        endforeach()
    endif()
    
    # Install completion files
    file(GLOB BASH_COMPLETIONS ${CMAKE_CURRENT_SOURCE_DIR}/completions/bash/*)
    file(GLOB ZSH_COMPLETIONS ${CMAKE_CURRENT_SOURCE_DIR}/completions/zsh/*)
    
    # Only install bash completions if the directory exists
    if(BASH_COMPLETIONS)
        if(BASH_COMPLETION_DIR AND EXISTS "${BASH_COMPLETION_DIR}")
            install(FILES ${BASH_COMPLETIONS}
                DESTINATION ${BASH_COMPLETION_DIR}
                COMPONENT "shell_completions")
            message(STATUS "Bash completions will be installed to: ${BASH_COMPLETION_DIR}")
        else()
            if(BASH_COMPLETION_DIR)
                message(STATUS "Bash completions directory not found: ${BASH_COMPLETION_DIR}")
            else()
                message(STATUS "No bash-completion directory found in standard locations")
            endif()
            message(STATUS "  Skipping bash completion installation. Use -DBASH_COMPLETION_DIR=<path> to specify a directory.")
        endif()
    endif()
    
    # Only install zsh completions if the directory exists
    if(ZSH_COMPLETIONS)
        if(ZSH_COMPLETION_DIR AND EXISTS "${ZSH_COMPLETION_DIR}")
            install(FILES ${ZSH_COMPLETIONS}
                DESTINATION ${ZSH_COMPLETION_DIR}
                COMPONENT "shell_completions")
            message(STATUS "Zsh completions will be installed to: ${ZSH_COMPLETION_DIR}")
        else()
            if(ZSH_COMPLETION_DIR)
                message(STATUS "Zsh completions directory not found: ${ZSH_COMPLETION_DIR}")
            else()
                message(STATUS "No zsh completion directory found in standard locations")
            endif()
            message(STATUS "  Skipping zsh completion installation. Use -DZSH_COMPLETION_DIR=<path> to specify a directory.")
        endif()
    endif()
endif()
