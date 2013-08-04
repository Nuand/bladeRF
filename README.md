# bladeRF Source #
This repository contains all the source code required to program and interact with a bladeRF platform, including firmware for the Cypress FX3 USB controller, HDL for the Altera Cyclone IV FPGA, and C code for the host side driver, library, and command line interface.

The source is organized as follows:

| Directory                     | Description                                                                                       |
| ----------------------------- |:--------------------------------------------------------------------------------------------------|
| [common][common]              | Source and header files common across the platform                                                |
| [fx3_firmware][fx3_firmware]  | Firmware for the Cypress FX3 USB controller                                                       |
| [hdl][hdl]                    | All HDL code associated with the Cyclone IV FPGA                                                  |
| [linux][linux]                | The linux kernel driver, library and command-line interface for interacting with a bladeRF device |

## Quick Start ##
It is not necessary to rebuild the Cypress FX3 firmware (hooray!), and there are pre-built FPGA images that will get you underway.

1. Compile and install the [kernel driver][linux_kernel], [libbladeRF library][linux_lib], and [command-line application][linux_apps], in that order.
2. Download pre-compiled FPGA binaries from [http://nuand.com/fpga][fpga_download].  You'll want `hostedx40.rbf` if you've got the 40 kLE board, or `hostedx115.rbf` if you have a 115 kLE board.
3. Attach the bladeRF board to your fastest USB port.
4. You should have a `/dev/bladerf0` character device file.  Make sure it is chowned and/or chmodded as you'd like it.  (Please see [`linux/kernel/README.md`][linux_kernel_readme] for a note about udev.)
5. Try `./bladeRF-cli -d /dev/bladerf0 -l /path/to/fpga.rbf` to load an FPGA image and open a dialogue with your bladeRF board, per the advice in [`linux/apps/README.md`][linux_apps_readme].

If you've made it this far, congratulations!  You can read (or write) raw samples in a variety of formats using the `bladeRF-cli` tool, or you can use third-party tools that support the bladeRF.

## Building for use with GNU Radio ##
[Directions for building and using this with GNU Radio][kb3gtn_guide] have been compiled by kb3gtn.

[kb3gtn_guide]: http://forums.nuand.com/forums/viewtopic.php?f=9&t=2804 (kb3gtn guide)

## Building Procedure ##
Each subsection of the source code can be built by itself and maintains their own README files for building.  The recommended build flow is as such:

- Build FX3 firmware under [`fx3_firmware`][fx3_firmware]
- Build FPGA image under [`hdl`][hdl]
- Build linux kernel driver under [`linux/kernel`][linux_kernel]
- Build linux libbladeRF library under [`linux/lib`][linux_lib]
- Build command-line application under [`linux/apps`][linux_apps]

[common]: ./common (Common)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[linux]: ./linux (Linux)
[linux_kernel]: ./linux/kernel (Linux Kernel Driver)
[linux_lib]: ./linux/lib (Linux Library)
[linux_apps]: ./linux/apps (Linux Apps)
[linux_kernel_readme]: ./linux/kernel/README.md (Linux Kernel Driver README)
[linux_apps_readme]: ./linux/apps/README.md (Linux Apps README)
[fpga_download]: http://nuand.com/fpga (nuand/FPGA Images)

