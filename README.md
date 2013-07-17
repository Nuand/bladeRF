# bladeRF Source #
This repository contains all the source code required to program and interact with a bladeRF platform, specifically the firmware for the Cypress FX3 USB controller, the HDL and C code for the Altera Cyclone IV FPGA and the host side driver, library and command line interface for simple interactions with the device.

The source is separated as follows:

| Directory                     | Description                                                                                       |
| ----------------------------- |:--------------------------------------------------------------------------------------------------|
| [common][common]              | Source and header files common across the platform                                                |
| [fx3_firmware][fx3_firmware]  | Firmware for the Cypress FX3 USB controller                                                       |
| [hdl][hdl]                    | All HDL code associated with the Cyclone IV FPGA                                                  |
| [linux][linux]                | The linux kernel driver, library and command-line interface for interacting with a bladeRF device |

## Building Procedure ##
Each subsection of the source code can be built by itself and maintains their own README files for building.  The recommended build flow is as such:

- Build FX3 firmware under [`fx3_firmware`][fx3_firmware]
- Build FPGA image under [`hdl`][hdl]
- Build linux kernel under [`linux/kernel`][linux_kernel]
- Build linux library under [`linux/lib`][linux_lib]
- Build command-line application under [`linux/apps`][linux_apps]

[common]: ./common (Common)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[linux]: ./linux (Linux)
[linux_kernel]: ./linux/kernel (Linux Kernel)
[linux_lib]: ./linux/lib (Linux Library)
[linux_apps]: ./linux/apps (Linux Apps)

