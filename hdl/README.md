# bladeRF HDL Source #
The Cyclone FPGA is at the heart of the bladeRF.  It interfaces to the Cypress FX3, Analog Devices AD9361 (bladeRF 2.0 micro) or Lime Micro LMS6002D (bladeRF 1) RF transceiver, Silicon Labs clock generator chip, and VCTCXO trim DAC.  It has an input to read NMEA from a GPS as well as a 1pps input for time synchronization of signals and has a reference input for a reference clock to tame the VCTCXO.  Lastly, it connects directly to the expansion header for controlling GPIO or any other interfaces that may reside on the expansion board.

The HDL is separated out into two different sections:

| Directory | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| fpga      | Source HDL in the form of IP blocks or platform specific top levels   |
| quartus   | Specific files for Quartus Prime project creation and building           |

IP, as it is added to the repository, falls under the category of who created the IP.  Some IP is created by Intel FPGA (formerly Altera) tools and remains in an `altera` directory.  Some has been written by Nuand or has been downloaded from OpenCores.  These blocks should be seen as independent from the platform and, essentially, building blocks for the entire platform.

Currently, the only platform we have is bladeRF.  As more platforms come out, more top levels will be created but the same IP should be able to be used with any of those platforms.

## Pre-compiled FPGA Binaries ##
Some FPGA binaries are available for [download][download].  Please note the md5 hash as well as the git commit hash.

[download]: https://www.nuand.com/fpga/ (nuand/FPGA Images)

## Required Software ##
We use an [Intel (Altera)][intel] [Cyclone V E FPGA][cve] for bladeRF 2.0 micro and a [Cyclone IV E FPGA][cive] for bladeRF 1 (classic).  The size of the FPGA is the only difference between different bladeRF models of the same generation (x40 vs x115, xA4 vs xA9).  Intel provides their [Quartus Prime][quartus] software for synthesizing designs for their FPGAs.  The Lite Edition is free of charge, but not open source, and may require registering on their site to download the software.

**Important Note:** Be sure to download and install [Quartus Prime version 20.1.1][quartus], which the bladeRF project files are based upon. Updates to Quartus Prime are not guaranteed to be reverse-compatible with earlier Quartus versions, nor will future versions necessarily be reverse-compatible. Particularly, newer versions of Quartus may not support older generation bladeRF 1 devices or the Nios II processor used on all bladeRF devices.

[intel]: https://www.altera.com/ (Altera, part of the Intel Programmable Solutions Group)
[quartus]: https://www.intel.com/content/www/us/en/software-kit/660904/intel-quartus-prime-lite-edition-design-software-version-20-1-1-for-linux.html (Quartus Prime Lite Edition v20.1.1)
[cve]: https://www.intel.com/content/www/us/en/products/details/fpga/cyclone/v.html
[cive]: https://www.intel.com/content/www/us/en/products/details/fpga/cyclone/iv.html

## HDL Structure ##
Since the FPGA is connected and soldered down to the board, it makes sense to have a single top level which defines where the pins go, their IO levels and their general directionality.  We use a single `bladerf.vhd` top level to define a VHDL entity called `bladerf` that defines these pins.

We realize people will want to change the internal guts of the FPGA for their own programmable logic reasons.  Because of this, we decided to differentiate the implementations using a feature of the Quartus Prime project file called Revisions.  Revisions can take a base design (top level entity, a part and pins) and duplicate that project, recording any source level changes you wish to make to the project.  This way, a user must only create their own architecture that is the new implementation of the `bladerf` top level.

This technique can be seen with the currently supported architectures:

| Architecture  | Description                                                                                                       |
| :------------ | :---------------------------------------------------------------------------------------------------------------- |
| hosted        | Listens for commands from the USB connection to perform operations or send/receive RF.                            |
| atsc_tx       | ATSC transmittter - Reads 4-bit ATSC symbols via USB and performs pilot insertion, filtering, and baseband shift. |

## Building the Project ##
The Quartus Prime build tools supports TCL as a scripting language which we utilize to not only create the project file, but build the system without requiring the need of the GUI. Currently, the `build_bladerf.sh` performs some environment checks, builds the NIOS BSP and software, and then kicks off TCL scripts to build the FPGA image.

To support multiple versions of Quartus Prime on the same machine and to ensure the environment is appropriately setup, please run the `nios2_command_shell.sh` script to get into an appropriate Quartus Prime environment.  Note that this shell script is usually located in the `nios2eds` directory of your Quartus Prime install directory (e.g. `~/intelFPGA_lite/20.1/nios2eds/nios2_command_shell.sh`).  Also note that this is the preferred method regardless of using Windows or Linux to build.  If you're using pybombs, make sure you haven't run `setup_env.sh`, as it prevents `nios2_command_shell.sh` from working properly.

1. Take note of your bladeRF model (bladeRF micro, bladeRF classic) and its corresponding FPGA size (x40, xA9, etc). You'll need this info below.
2. Enter the `quartus` directory
3. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -h` to view the usage for the build script. Note the board, size, and revision options. Also note any items you'll need to add to your PATH before continuing.
4. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -b <board> -s <size> -r <revision>`, with your bladeRF board model, the relevant size for your bladeRF, and the desired revision.  This will create the NIOS system and software associated with the FPGA build needed by the internal RAM for execution.
5. The output of the build will be stored in a directory named in the form, `<revision>x<size>-<build time>`.  This directory will contain the bitstream, summaries, and reports.

Note that there will be a _lot_ of information displayed from notes to critical warnings.  Some of these are benign and others are, in fact, critical.

Ex: Build the bladeRF 2.0 micro xA4 with the default `hosted` architecture
```
./build_bladerf.sh -b bladeRF-micro -s A4 -r hosted
```

## Adding Signal Tap ##
For advanced users who want to use Signal Tap internal logic analyzer in their design, use the `-a` flag to add the Signal Tap file to the project before building it:
```
./build_bladerf.sh -b bladeRF-micro -s A4 -r hosted -a ../signaltap/debug_rx.stp
```

Note that to use Signal Tap with the Quartus Prime Lite Edition software, the TalkBack feature must be enabled.  The build script tries to 'fake' this out by setting the TalkBack feature to be on, compiling the project, then turning it off immediately afterward.  If this behavior is not desired, don't try to add a Signal Tap file to the project.
