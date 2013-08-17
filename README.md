# bladeRF Source #
This repository contains all the source code required to program and interact with a bladeRF platform, including firmware for the Cypress FX3 USB controller, HDL for the Altera Cyclone IV FPGA, and C code for the host side libraries, drivers, and utilities.

The source is organized as follows:

| Directory                     | Description                                                                                       |
| ----------------------------- |:--------------------------------------------------------------------------------------------------|
| [common][common]              | Source and header files common between the following components                                   |
| [fx3_firmware][fx3_firmware]  | Firmware for the Cypress FX3 USB controller                                                       |
| [hdl][hdl]                    | All HDL code associated with the Cyclone IV FPGA                                                  |
| [host][host]                  | Host-side libraries, drivers, utilities and samples                                               |

<!--
## Quick Start ##
It is not necessary to rebuild the Cypress FX3 firmware (hooray!), and there are pre-built FPGA images that will get you underway.

1. Compile and install the [kernel driver][linux_kernel], [libbladeRF library][linux_lib], and [command-line application][linux_apps], in that order.
2. Download pre-compiled FPGA binaries from [http://nuand.com/fpga][fpga_download].  You'll want `hostedx40.rbf` if you've got the 40 kLE board, or `hostedx115.rbf` if you have a 115 kLE board.
3. Attach the bladeRF board to your fastest USB port.
4. You should have a `/dev/bladerf0` character device file.  Make sure it is chowned and/or chmodded as you'd like it.  (Please see [`linux/kernel/README.md`][linux_kernel_readme] for a note about udev.)
5. Try `./bladeRF-cli -d /dev/bladerf0 -l /path/to/fpga.rbf` to load an FPGA image and open a dialogue with your bladeRF board, per the advice in [`linux/apps/README.md`][linux_apps_readme].
-->

[common]: ./common (Common)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[host]: ./host (Host)
[linux_kernel]: ./linux/kernel (Linux Kernel Driver)
[linux_lib]: ./linux/lib (Linux Library)
[linux_apps]: ./linux/apps (Linux Apps)
[linux_kernel_readme]: ./linux/kernel/README.md (Linux Kernel Driver README)
[linux_apps_readme]: ./linux/apps/README.md (Linux Apps README)
[fpga_download]: http://nuand.com/fpga (nuand/FPGA Images)
