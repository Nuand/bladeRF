cmake_minimum_required(VERSION 2.8)
project(libbladeRF_doc_examples C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/include
)
set(LIBS libbladerf_shared)

if(NOT MSVC)
    set (LIBS ${LIBS} m)
endif()


add_executable(libbladeRF_example_sync_rxtx sync_rxtx.c example_common.c)
target_link_libraries(libbladeRF_example_sync_rxtx ${LIBS})

add_executable(libbladeRF_example_sync_rx_meta sync_rx_meta.c example_common.c)
target_link_libraries(libbladeRF_example_sync_rx_meta ${LIBS})

add_executable(libbladeRF_example_sync_tx_meta sync_tx_meta.c example_common.c)
target_link_libraries(libbladeRF_example_sync_tx_meta ${LIBS})

add_executable(libbladeRF_example_open_via_serial open_via_serial.c example_common.c)
target_link_libraries(libbladeRF_example_open_via_serial ${LIBS})
