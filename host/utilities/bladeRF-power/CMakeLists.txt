cmake_minimum_required(VERSION 3.10...3.27)
project(bladeRF-power LANGUAGES C)

set (CURSES_NEED_NCURSES TRUE)
set (CURSES_NEED_WIDE TRUE)

if(APPLE)
    if(EXISTS "/opt/homebrew/opt/ncurses")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/ncurses")
    elseif(EXISTS "/usr/local/opt/ncurses")
        list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/ncurses")
    else()
        message(WARNING "Homebrew ncurses not found. Install it if the build is failing: brew install ncurses")
    endif()
endif()

find_package(Curses REQUIRED)

include_directories(
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${libbladeRF_SOURCE_DIR}/include
    ./include)

add_executable(${PROJECT_NAME}
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    src/init.c
    src/helpers.c
    src/window.c
    src/filter.c
    src/text.c
    src/main.c)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${PROJECT_NAME}
    libbladerf_shared
    m
    ${BLADERF_HOST_COMMON_LIBRARIES}
    ${CURSES_LIBRARIES}
    ncurses)

install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})
