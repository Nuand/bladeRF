# bladeRF HDL Source #
The Cyclone IV FPGA is at the heart of the bladeRF.  It interfaces to the Cypress FX3, Lime Micro LMS6002D RF transceiver, Si5338 clock generator chip and VCTCXO trim DAC.  It has an input to read NMEA from a GPS as well as a 1pps input for time synchronization of signals and has a reference input for a reference clock to tame the VCTCXO.  Lastly, it connects directly to the expansion header for controlling GPIO or any other interfaces that may reside on the expansion board.

The HDL is separated out into two different sections:

| Directory | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| fpga      | Source HDL in the form of IP blocks or platform specific top levels   |
| quartus   | Specific files for Quartus II project creation and building           |

IP, as it is added to the repository, falls under the category of who created the IP.  Some IP is created by Altera tools and remains in an Altera directory.  Some has been written by nuand or has been downloaded from OpenCores.  These blocks should be seen as independant from the platform and, essentially, building blocks for the entire platform.

Currently, the only platform we have is bladeRF.  As more platforms come out, more top levels will be created but the same IP should be able to be used with any of those platforms.

## Required Software ##
We use an [Altera][altera] [Cyclone IV E FPGA][cive].  The size of the FPGA is the only difference between the x40 and x115 models.  Altera provides their [Quartus II software][quartus] for synthesizing designs for their FPGAs.  It is free of charge, but not open source and may require registering on their site to download the software.

[altera]: http://www.altera.com (Altera)
[quartus]: http://www.altera.com/products/software/quartus-ii/web-edition/qts-we-index.html (Quartus II Web Edition Software)
[cive]: http://www.altera.com/devices/fpga/cyclone-iv/overview/cyiv-overview.html

## HDL Structure ##
Since the FPGA is connected and soldered down to the board, it makes sense to have a single top level which defines where the pins go, their IO levels and their genera directionality.  We use a single `bladerf.vhd` top level to define a VHDL entity called `bladerf` that defines these pins.

We realize people will want to change the internal guts of the FPGA for their own programmable logic reasons.  Because of this, we decided to differentiate the implementations using a feature of the Quartus II project file called Revisions.  Revisions can take a base design (top level entity, a part and pins) and duplicate that project, recording any source level changes you wish to make to the project.  This way, a user must only create their own architecture that is the new implementation of the `bladerf` top level.

This technique can be seen with the different architectures:

| Architecture  | Description                                                                           |
| :------------ | :------------------------------------------------------------------------------------ |
| fsk_bridge    | Implements a serial device which is connected via FSK to another bladeRF              |
| headless      | Does not require a USB connection to a host and is fully autonomous                   |
| hosted        | Listens for commands from the USB connection to perform operations or send/receive RF |
| qpsk_tx       | A debug implementation which just transmits 3.84Msps QPSK which has been RRC filtered |

## Building the Project ##
The Quartus II build tools supports TCL as a scripting language which we utilize to not only create the project file, but build the system without requiring the need of the GUI.

1. Execute the `build_bladerf.sh` script in the `quartus` directory: `cd quartus; ./build_bladerf.sh; cd ..`  This will create the NIOS system and software associated with the FPGA build needed by the internal RAM for execution.
1. Create a work directory for the project: `mkdir -p quartus/work ; cd quartus/work`
1. Create the bladeRF project with all the revisions: `quartus_sh -t ../bladerf.tcl`
1. Build a specific revision of the bladeRF project: `quartus_sh -t ../build.tcl -rev hosted -size 40`
1. Look for the file `output_files/<revision>.rbf` which is the FPGA programming image.

Note that there will be a _lot_ of information displayed from notes to critical warnings.  Some of these are benign and others are, in fact, critical.

## Adding Signal Tap ##
For advanced users who want to use Signal Tap internal logic analyzer in their design, the `build.tcl` file can be passed a `-stp` flag which will add the Signal Tap file to the project before building it:

```
quartus_sh -t ../build.tcl -rev hosted -size 115 -stp ../signaltap/debug_rx.stp
```
## Pre-compiled FPGA Binaries ##
We currently do not provide pre-compiled binaries of the FPGA firmware due to the volatility of the FPGA code.  This will be rectified in the near future.
