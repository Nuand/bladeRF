# bladeRF HDL Source #
The Cyclone FPGA is at the heart of the bladeRF.  It interfaces to the Cypress FX3, RF transceiver, clock generator chip, and VCTCXO trim DAC.  It has an input to read NMEA from a GPS as well as a 1pps input for time synchronization of signals and has a reference input for a reference clock to tame the VCTCXO.  Lastly, it connects directly to the expansion header for controlling GPIO or any other interfaces that may reside on the expansion board.

The HDL is separated out into two different sections:

| Directory | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| fpga      | Source HDL in the form of IP blocks or platform specific top levels   |
| quartus   | Specific files for Quartus Prime project creation and building        |

IP, as it is added to the repository, falls under the category of who created the IP.  Some IP is created by Intel FPGA (formerly Altera) tools and remains in an `altera` directory.  Some has been written by Nuand or has been downloaded from OpenCores.  These blocks should be seen as independent from the platform and, essentially, building blocks for the entire platform.

Currently, there is two platforms of bladeRF, both based on [Intel (Altera)][intel] [Cyclone FPGA][cyclone]:

|         Platform          |       FPGA         |               RFIC              |
| :------------------------ | :----------------- | :------------------------------ |
| Bladerf x40/x115          | [Cyclone IV][cive] | [Lime Micro LMS6002D][lms]      |
| Bladerf 2.0 micro xA4/xA9 | [Cyclone V][cve]   | [Analog Devices AD9361][ad9361] |

As more platforms come out, more top levels will be created but the same IP should be able to be used with any of those platforms.

[cyclone]: https://www.intel.com/content/www/us/en/products/programmable/cyclone-series.html
[cive]: https://www.intel.com/content/www/us/en/products/programmable/fpga/cyclone-iv.html
[cve]: https://www.intel.com/content/www/us/en/products/programmable/fpga/cyclone-v.html
[lms]: https://limemicro.com/technology/lms6002d/
[ad9361]: https://www.analog.com/en/products/ad9361.html

## Pre-compiled FPGA Binaries ##
Some FPGA binaries are available for [download][download].  Please note the md5 hash as well as the git commit hash.

[download]: https://www.nuand.com/fpga/ (nuand/FPGA Images)

## Required Software ##
Intel provides their [Quartus Prime][quartus] software for synthesizing designs for their FPGAs.  The Lite Edition is free of charge, but not open source, and require registering on their site to download the software.

**Important Note:** Be sure to download and install [Quartus Prime version 17.1][quartus], which the bladeRF project files are based upon. Updates to Quartus Prime are not guaranteed to be reverse-compatible with earlier Quartus versions, nor will future versions necessarily be reverse-compatible.

[intel]: https://www.altera.com/ (Altera, part of the Intel Programmable Solutions Group)
[quartus]: https://dl.altera.com/17.1/?edition=lite (Quartus Prime Lite Edition v17.1)

## HDL Structure ##
Since the FPGA is connected and soldered down to the board, it makes sense to have a single top level which defines where the pins go, their IO levels and their genera directionality.  We use a single `bladerf.vhd` top level to define a VHDL entity called `bladerf` that defines these pins.

We realize people will want to change the internal guts of the FPGA for their own programmable logic reasons.  Because of this, we decided to differentiate the implementations using a feature of the Quartus Prime project file called Revisions.  Revisions can take a base design (top level entity, a part and pins) and duplicate that project, recording any source level changes you wish to make to the project.  This way, a user must only create their own architecture that is the new implementation of the `bladerf` top level.

This technique can be seen with the currently supported architectures:

| Architecture  | Description                                                                                                       |
| :------------ | :---------------------------------------------------------------------------------------------------------------- |
| hosted        | Listens for commands from the USB connection to perform operations or send/receive RF.                            |
| atsc_tx       | ATSC transmittter - Reads 4-bit ATSC symbols via USB and performs pilot insertion, filtering, and baseband shift. |
| adsb          | ADSB receiver - detect and resolve many bit errors in real-time and sends via USB the decoded data                |

Note: Other architecture revisions like fsk_bridge, headless and qpsk_tx are available, but only for the bladerf x40/x115. 

## Building the Project ##
The Quartus Prime build tools supports TCL as a scripting language which we utilize to not only create the project file, but build the system without requiring the need of the GUI. Currently, the `build_bladerf.sh` performs some environment checks, builds the NIOS BSP and software, and then kicks off TCL scripts to build the FPGA image.

To support multiple versions of Quartus Prime on the same machine and to ensure the environment is appropriately setup, please use the `nios2_command_shell.sh` script to get into an appropriate Quartus Prime environment.  Note that this shell script is usually located in the `nios2eds` directory of your Quartus Prime install directory.  Also note that this is the preferred method regardless of using Windows or Linux to build.  If you're using pybombs, make sure you haven't run `setup_env.sh`, as it prevents `nios2_command_shell.sh` from working properly.

1. Take note of which Cyclone device you have, you'll need it for the size parameter.
2. Enter the `quartus` directory
3. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -h` to view the usage for the build script. Note the size and revision options. Also note any items you'll need to add to your PATH before continuing.
4. Execute, from inside an appropriate NIOS II command shell, `./build_bladerf.sh -s <size> -r <revision>`, with the relevant size for your bladeRF, and the desired revision.  This will create the NIOS system and software associated with the FPGA build needed by the internal RAM for execution.
5. The output of the build will be stored in a directory named in the form, `<revision>x<size>-<build time>`.  This directory will contain the bitstream, summaries, and reports.

The script should give this output:
```
$ ./build_bladerf.sh

bladeRF FPGA build script

Usage: build_bladerf.sh -b <board_name> -r <rev> -s <size>

Options:
    -c                    Clear working directory
    -b <board_name>       Target board name
    -r <rev>              Quartus project revision
    -s <size>             FPGA size
    -a <stp>              SignalTap STP file
    -f                    Force SignalTap STP insertion by temporarily enabling
                          the TalkBack feature of Quartus (required for Web Edition).
                          The previous setting will be restored afterward.
    -n <Tiny|Fast>        Select Nios II Gen 2 core implementation.
       Tiny (default)       Nios II/e; Compatible with Quartus Web Edition
       Fast                 Nios II/f; Requires Quartus Standard or Pro Edition
    -l <gen|synth|full>   Quartus build steps to complete. Valid options:
       gen                  Only generate the project files
       synth                Synthesize the design
       full (default)       Fit the design and create programming files
    -S <seed>             Fitter seed setting (default: 1)
    -h                    Show this text

Supported boards:
    [*] bladeRF-micro
        Supported revisions:
            hosted
            adsb
        Supported sizes (kLE):
            A2
            A4
            A9

    [*] bladeRF
        Supported revisions:
            hosted
            adsb
            atsc_tx
            fsk_bridge
            headless
            qpsk_tx
        Supported sizes (kLE):
            40
            115
```


Note that there will be a _lot_ of information displayed from notes to critical warnings.  Some of these are benign and others are, in fact, critical.

## Adding Signal Tap ##
For advanced users who want to use Signal Tap internal logic analyzer in their design, the `build.tcl` file can be passed a `-stp` flag which will add the Signal Tap file to the project before building it:

```
quartus_sh -t ../build.tcl -rev hosted -size 115 -stp ../signaltap/debug_rx.stp
```

Note that to use Signal Tap with the Quartus Prime Lite Edition software, the TalkBack feature must be enabled.  The build script tries to 'fake' this out by setting the TalkBack feature to be on, compiling the project, then turning it off immediately afterward.  If this behavior is not desired, don't try to add a Signal Tap file to the project.
