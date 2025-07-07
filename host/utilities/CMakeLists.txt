cmake_minimum_required(VERSION 3.10...3.27)

add_subdirectory(bladeRF-cli)
add_subdirectory(bladeRF-update)
if(NOT MSVC)
    add_subdirectory(bladeRF-power)
    add_subdirectory(bladeRF-fsk/c)
endif()
