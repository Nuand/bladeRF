#!/usr/bin/env bash

ip_generate=`dirname \`which quartus_sh\``/../sopc_builder/bin/ip-generate
nios_system=../fpga/ip/altera/nios_system

echo "Generating NIOS II Qsys for bladeRF ..."
$ip_generate \
    --project-directory=$nios_system \
    --output-directory=$nios_system \
    --report-file=bsf:$nios_system/nios_system.bsf \
    --system-info=DEVICE_FAMILY=Cyclone\ IV\ E \
    --system-info=DEVICE=EP4CE40F23C8 \
    --system-info=DEVICE_SPEEDGRADE=8 \
    --component-file=$nios_system/nios_system.qsys

$ip_generate \
    --project-directory=$nios_system \
    --output-directory=$nios_system/synthesis \
    --file-set=QUARTUS_SYNTH \
    --report-file=sopcinfo:$nios_system/nios_system.sopcinfo \
    --report-file=html:$nios_system/nios_system.html \
    --report-file=qip:$nios_system/synthesis/nios_system.qip \
    --report-file=cmp:$nios_system/nios_system.cmp \
    --system-info=DEVICE_FAMILY=Cyclone\ IV\ E \
    --system-info=DEVICE=EP4CE40F23C8 \
    --system-info=DEVICE_SPEEDGRADE=8 \
    --component-file=$nios_system/nios_system.qsys

echo "Synthesizing bladeRF FPGA..."
quartus_sh -t bladerf.tcl

echo "Completed!"
