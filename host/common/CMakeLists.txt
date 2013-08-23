cmake_minimum_required(VERSION 2.8)
project(bladerf_host_common C)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/host_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/host_config.h
    @ONLY
)
