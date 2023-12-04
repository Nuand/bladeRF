cmake_minimum_required(VERSION 3.5)

add_subdirectory(bladeRF-cli)

if(NOT MSVC)
    add_subdirectory(bladeRF-fsk/c)
endif()