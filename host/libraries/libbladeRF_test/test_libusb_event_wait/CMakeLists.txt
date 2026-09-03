cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_libusb_event_wait C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${BLADERF_FPGA_COMMON_INCLUDE_DIR}
    ${libbladeRF_BINARY_DIR}/src
    ${LIBUSB_INCLUDE_DIRS}
)

include_directories(${INCLUDES})

find_package(Threads REQUIRED)

add_executable(libbladeRF_test_libusb_event_wait
    main.c
    ${libbladeRF_SOURCE_DIR}/src/backend/usb/libusb.c
)
target_compile_definitions(libbladeRF_test_libusb_event_wait PRIVATE
    BLADERF_TEST_LIBUSB_EVENT_WAIT
)
target_compile_options(libbladeRF_test_libusb_event_wait PRIVATE
    -ffunction-sections
    -ULOGGING_ENABLED
    -Wno-unused-function
    -Wno-unused-variable
)
target_link_options(libbladeRF_test_libusb_event_wait PRIVATE
    -Wl,--gc-sections
    -Wl,--wrap=libusb_handle_events_timeout
    -Wl,--wrap=libusb_handle_events_timeout_completed
)
target_link_libraries(libbladeRF_test_libusb_event_wait
    ${LIBUSB_LIBRARIES}
    Threads::Threads
)
