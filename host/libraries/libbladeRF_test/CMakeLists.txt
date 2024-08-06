cmake_minimum_required(VERSION 3.9)

add_subdirectory(test_async)
add_subdirectory(test_bootloader_recovery)
add_subdirectory(test_c)
#add_subdirectory(test_config_file)
add_subdirectory(test_clock_select)
add_subdirectory(test_cpp)
add_subdirectory(test_ctrl)
add_subdirectory(test_freq_hop)
add_subdirectory(test_fw_check)
add_subdirectory(test_open)
add_subdirectory(test_parse)
add_subdirectory(test_peripheral_timing)
add_subdirectory(test_gain_compare)
add_subdirectory(test_gain_calibration)
add_subdirectory(test_repeater)
add_subdirectory(test_quick_retune)
add_subdirectory(test_repeated_stream)
add_subdirectory(test_rx_discont)
add_subdirectory(test_scheduled_retune)
add_subdirectory(test_sync)
add_subdirectory(test_timestamps)
add_subdirectory(test_tune_timing)
add_subdirectory(test_unused_sync)
add_subdirectory(test_version)
add_subdirectory(test_digital_loopback)
add_subdirectory(test_interleaver)
add_subdirectory(test_rx_meta)
add_subdirectory(test_fpga_load)

option(TEST_REGRESSION "Include regression tests" OFF)
if(TEST_REGRESSION)
    add_subdirectory(test_regression)
endif()

configure_file(requirements.txt
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/requirements.txt
               FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ
               COPYONLY)

configure_file(setup_env.sh
               ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/setup_env.sh
               FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ
               COPYONLY)
