cmake_minimum_required(VERSION 3.5)
project(bladeRF-power LANGUAGES C)

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
    src/main.c)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${PROJECT_NAME}
    libbladerf_shared
    m
    ${BLADERF_HOST_COMMON_LIBRARIES}
    ${CURSES_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})
