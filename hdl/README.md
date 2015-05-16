# bladeRF HDL Source #
The Cyclone IV FPGA is at the heart of the bladeRF.  It interfaces to the Cypress FX3, Lime Micro LMS6002D RF transceiver, Si5338 clock generator chip, and VCTCXO trim DAC.  It has an input to read NMEA from a GPS as well as a 1pps input for time synchronization of signals and has a reference input for a reference clock to tame the VCTCXO.  Lastly, it connects directly to the expansion header for controlling GPIO or any other interfaces that may reside on the expansion board.

The HDL is separated out into two different sections:

| Directory | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| fpga      | Source HDL in the form of IP blocks or platform specific top levels   |
| quartus   | Specific files for Quartus II project creation and building           |

IP, as it is added to the repository, falls under the category of who created the IP.  Some IP is created by Altera tools and remains in an Altera directory.  Some has been written by nuand or has been downloaded from OpenCores.  These blocks should be seen as independent from the platform and, essentially, building blocks for the entire platform.

Currently, the only platform we have is bladeRF.  As more platforms come out, more top levels will be created but the same IP should be able to be used with any of those platforms.

## Pre-compiled FPGA Binaries ##
Some FPGA binaries are available for [download][download].  Please note the md5 hash as well as the git commit hash.

[download]: http://nuand.com/fpga (nuand/FPGA Images)

## Required Software ##
We use an [Altera][altera] [Cyclone IV E FPGA][cive].  The size of the FPGA is the only difference between the x40 and x115 models.  Altera provides their [Quartus II][quartus] software for synthesizing designs for their FPGAs.  It is free of charge, but not open source and may require registering on their site to download the software.

**Important Note:** Be sure to download [Quartus II version 15.0][quartus], which the bladeRF project files are based upon. The updates to Quartus II 15.0 are not reverse-compatible with earlier Quartus versions (e.g., 14.0, 13.1).

[altera]: http://www.altera.com (Altera)
[quartus]: http://dl.altera.com/15.0/?edition=web (Quartus II Web Edition 15.0 Software)
[cive]: http://www.altera.com/devices/fpga/cyclone-iv/overview/cyiv-overview.html

## HDL Structure ##
Since the FPGA is connected and soldered down to the board, it makes sense to have a single top level which defines where the pins go, their IO levels and their genera directionality.  We use a single `bladerf.vhd` top level to define a VHDL entity called `bladerf` that defines these pins.

We realize people will want to change the internal guts of the FPGA for their own programmable logic reasons.  Because of this, we decided to differentiate the implementations using a feature of the Quartus II project file called Revisions.  Revisions can take a base design (top level entity, a part and pins) and duplicate that project, recording any source level changes you wish to make to the project.  This way, a user must only create their own architecture that is the new implementation of the `bladerf` top level.

This technique can be seen with the currently supported architectures:

| Architecture  | Description                                                                                                       |
| :------------ | :---------------------------------------------------------------------------------------------------------------- |
| hosted        | Listens for commands from the USB connection to perform operations or send/receive RF.                            |
| atsc_tx       | ATSC transmittter - Reads 4-bit ATSC symbols via USB and performs pilot insertion, filtering, and baseband shift. |

## Building the Project ##
The Quartus II build tools supports TCL as a scripting language which we utilize to not only create the project file, but build the system without requiring the need of the GUI. Currently, the `build_bladerf.sh` performs some environment checks, builds the NIOS BSP and software, and then kicks off TCL scripts to build the FPGA image.

To support multiple versions of Quartus II on the same machine and to ensure the environment is appropriately setup, please use the `nios2_command_shell.sh` script to get into an appropriate Quartus II environment.  Note that this shell script is usually located in the `nios2eds` directory of your Quartus II install directory.  Also note that this is the preferred method regardless of using Windows or Linux to build.  If you're using pybombs, make sure you haven't run `setup_env.sh`, as it prevents `nios2_command_shell.sh` from working properly.

1. Take note of which Altera Cyclone IV you have. (The EP4CE40 is 40 kLE, and the EP4CE115 is 115 kLE.)  You'll need this size below...
2. Enter the `quartus` directory
3. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -h` to view the usage for the build script. Note the size and revision options. Also note any items you'll need to add to your PATH before continuing.
4. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -s <size> -r <revision>`, with the relevant size for your bladeRF, and the desired revision.  This will create the NIOS system and software associated with the FPGA build needed by the internal RAM for execution.
5. The output of the build will be stored in a directory named in the form, `<revision>x<size>-<build time>`.  This directory will contain the bitstream, summaries, and reports.

Note that there will be a _lot_ of information displayed from notes to critical warnings.  Some of these are benign and others are, in fact, critical.

## Adding Signal Tap ##
For advanced users who want to use Signal Tap internal logic analyzer in their design, the `build.tcl` file can be passed a `-stp` flag which will add the Signal Tap file to the project before building it:

```
quartus_sh -t ../build.tcl -rev hosted -size 115 -stp ../signaltap/debug_rx.stp
```

Note that to use Signal Tap with the Quartus II Web Edition software, Altera requires that the TalkBack feature be enabled.  The build script tries to 'fake' this out by setting the TalkBack feature to be on, compiling the project, then turning it off immediately.  If this behavior is not desired, don't try to add a Signal Tap file to the project.
