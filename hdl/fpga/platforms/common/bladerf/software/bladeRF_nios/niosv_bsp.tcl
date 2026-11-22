# Nios V HAL BSP settings for the bladeRF Nios firmware (niosv-bsp --script).
# These mirror the Nios II BSP options in hdl/quartus/build_bladerf.sh, so both
# CPUs run the firmware with the same footprint-oriented HAL configuration.

set_setting hal.max_file_descriptors 4
set_setting hal.enable_instruction_related_exceptions_api false
set_setting hal.make.cflags_optimization -Os
set_setting hal.enable_exit 0
set_setting hal.enable_clean_exit 0
set_setting hal.enable_c_plus_plus 0
set_setting hal.enable_reduced_device_drivers true
set_setting hal.enable_lightweight_device_driver_api true

# The Nios II BSP is built without stack-protector code; match it.
set_setting hal.make.enable_cflag_fstack_protector_strong 0

# The Nios II system has no timer, so its HAL has no system tick or timestamp
# timer. With the Nios V machine timer connected, the defaults would add a
# 1 kHz tick interrupt; keep the firmware free of it. usleep() still uses the
# machine timer (mtime) directly.
set_setting hal.sys_clk_timer none
set_setting hal.timestamp_timer none
