cmake_minimum_required(VERSION 3.5)
project(libbladeRF_test_gain_calibration C)

set(INCLUDES
    ./include
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(LIBBLADERF_SEARCH_PREFIX_OVERRIDE)
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${LIBBLADERF_SEARCH_PREFIX_OVERRIDE}")
else()
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")
endif()

set(SRC
    src/main.c
    src/file_operations.c
    ${libbladeRF_SOURCE_DIR}/src/helpers/file.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

include_directories(${INCLUDES})
add_executable(libbladeRF_test_gain_calibration ${SRC})
target_link_libraries(libbladeRF_test_gain_calibration libbladerf_shared m)

configure_file(example_measurements/rx_sweep_0.csv
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rx_sweep_0.csv
               FILE_PERMISSIONS OWNER_READ
               COPYONLY)

configure_file(example_measurements/tx_sweep_60.csv
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/tx_sweep_60.csv
               FILE_PERMISSIONS OWNER_READ
               COPYONLY)

configure_file(example_measurements/tx_sweep_60_fine.csv
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/tx_sweep_60_fine.csv
               FILE_PERMISSIONS OWNER_READ
               COPYONLY)

configure_file(example_measurements/rx_sweep.csv
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/rx_sweep.csv
               FILE_PERMISSIONS OWNER_READ
               COPYONLY)

configure_file(example_measurements/tx_sweep.csv
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/tx_sweep.csv
               FILE_PERMISSIONS OWNER_READ
               COPYONLY)
