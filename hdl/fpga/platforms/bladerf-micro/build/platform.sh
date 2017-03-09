#!/usr/bin/env bash

# FX3 PLL
echo "Build FX3 PLL"
ip-generate \
    --output-directory="fx3_pll" \
    --output-name="fx3_pll" \
    --remove-qsys-generate-warning \
    --file-set=QUARTUS_SYNTH \
    --language=VHDL \
    --component-name="altera_pll" \
    --part=${DEVICE} \
    --component-param=gui_pll_mode="Integer-N PLL" \
    --component-param=gui_operation_mode="source synchronous" \
    --component-param=gui_pll_auto_reset="On" \
    --component-param=gui_use_locked="True" \
    --component-param=gui_number_of_clocks="1" \
    --component-param=gui_reference_clock_frequency="100.0 MHz" \
    --component-param=gui_output_clock_frequency0="100.0 MHz" \
    --component-param=gui_phase_shift0="-3420 ps" \
    --component-param=gui_duty_cycle0="50%" \

# System PLL
echo "Build System PLL"
ip-generate \
    --output-directory="system_pll" \
    --output-name="system_pll" \
    --remove-qsys-generate-warning \
    --file-set=QUARTUS_SYNTH \
    --language=VHDL \
    --component-name="altera_pll" \
    --part=${DEVICE} \
    --component-param=gui_pll_mode="Integer-N PLL" \
    --component-param=gui_operation_mode="normal" \
    --component-param=gui_pll_auto_reset="On" \
    --component-param=gui_use_locked="True" \
    --component-param=gui_number_of_clocks="1" \
    --component-param=gui_reference_clock_frequency="38.4 MHz" \
    --component-param=gui_output_clock_frequency0="80.0 MHz" \
    --component-param=gui_phase_shift0="0.0 ps" \
    --component-param=gui_duty_cycle0="50%" \

