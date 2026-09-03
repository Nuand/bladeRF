cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_sync_worker_stop C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${BLADERF_FPGA_COMMON_INCLUDE_DIR}
    ${libbladeRF_BINARY_DIR}/src
)

include_directories(${INCLUDES})

find_package(Threads REQUIRED)

add_executable(libbladeRF_test_sync_worker_stop
    main.c
    ${libbladeRF_SOURCE_DIR}/src/streaming/sync_worker.c
)
target_compile_options(libbladeRF_test_sync_worker_stop PRIVATE
    -ffunction-sections
)
target_link_options(libbladeRF_test_sync_worker_stop PRIVATE
    -Wl,--gc-sections
)
target_link_libraries(libbladeRF_test_sync_worker_stop Threads::Threads)
