cmake_minimum_required(VERSION 3.5)
project(libbladeRF_doc_examples C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/include
)
set(LIBS libbladerf_shared)

set(SRC
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES})

add_executable(libbladeRF_example_sync_rxtx sync_rxtx.c example_common.c)
target_link_libraries(libbladeRF_example_sync_rxtx ${LIBS})

add_executable(libbladeRF_example_sync_rx_meta sync_rx_meta.c example_common.c ${SRC})
target_link_libraries(libbladeRF_example_sync_rx_meta ${LIBS})

add_executable(libbladeRF_example_sync_tx_meta sync_tx_meta.c example_common.c)
target_link_libraries(libbladeRF_example_sync_tx_meta ${LIBS})

add_executable(libbladeRF_example_open_via_serial open_via_serial.c example_common.c)
target_link_libraries(libbladeRF_example_open_via_serial ${LIBS})

add_executable(libbladeRF_example_boilerplate boilerplate.c)
target_link_libraries(libbladeRF_example_boilerplate ${LIBS})

add_executable(libbladeRF_example_quick_tune quick_tune.c)
target_link_libraries(libbladeRF_example_quick_tune ${LIBS})

add_executable(libbladeRF_example_freq_range_sweep freq_range_sweep.c)
target_link_libraries(libbladeRF_example_freq_range_sweep ${LIBS})
