bladeRF NIOS II software
==========================

The [bladeRF_nios](./bladeRF_nios) directory contains the bladeRF program that executes on a NIOS II soft processor in the FPGA. This program is responsible for performing control and configuration operations requests from the host, as received via the FX3 UART.  Based upon the request information, the operation is applied to a physical device attached to the FPGA, or a module in the FPGA's programmable fabric.

Packet Formats
========================

Requests and responses are sent/received in 16-byte packets.  The first byte in the p is a "magic" value that denotes the packet format type. The following bytes are specific to the packet format.  See the pkt_*.h files in the [fpga_common/include/](../../../../../../fpga_common/include) directory for complete descriptions of these formats.

IDs and "Magic" header byte values are reserved for user customization; official releases will not use these IDs. The below tables briefly describe the IDs used for each of the packet formats.

Magic Byte   | Packet format
-------------|----------------------
    0x00     | Do not use
  0x01-0x40  | Reserved for offical bladeRF packet formats
    0x41     | pkt_8x8
    0x42     | pkt_8x16
    0x43     | pkt_8x32
    0x44     | pkt_8x64
  0x45-0x4a  | Reserved for offical bladeRF packet formats
    0x4b     | pkt_32x32
  0x4c-0x4d  | Reserved for offical bladeRF packet formats
    0x4e     | pkt_legacy
  0x4f-53    | Reserved for offical bladeRF packet formats
    0x54     | pkt_retune
  0x55-0x7f  | Reserved for offical bladeRF packet formats
  0x80-0xff  | Reserved for user customization

**pkt_8x8** : 8-bit address, 8-bit address accesses


          ID | Peripheral/Device/Block
------------ | -------------
0x00         | LMS6002D register access
0x01         | SI5338 register access
0x02         | VCTCXO tamer module. Subaddress 0xff accesses mode selection.
0x80-0xff    | Reserved for user customization


**pkt_8x16**: 8-bit address, 16-bit data accesses

          ID | Peripheral/Device/Block
------------ | -------------
0x00         | VCTCXO Trim DAC register access
0x01         | IQ Correction
0x80-0xff    | Reserved for user customization


**pkt_8x32**: 8-bit address, 32-bit data access

           ID | Peripheral/Device/Block
------------ | -------------
0x00        | FPGA Control Register
0x01        | FPGA Version (Read only)
0x02        | XB200 ADF4351 register access
0x80-0xff  | Reserved for user customization

**pkt_8x64**: 8-bit address, 64-bit data access

           ID | Peripheral/Device/Block
------------ | -------------
0x00        | RX Timestamp (read-only)
0x01        | TX Timestamp (read-only)
0x80-0xff  | Reserved for user customization

**pkt_32x32**: 32-bit address (or mask), 32-bit data access

           ID | Peripheral/Device/Block
------------ | -------------
0x00         | Expansion I/O
0x01         | Expansion I/O direction
0x80-0xff    | Reserved for user customization

**pkt_retune**: Does not have ID fields available for use

**pkt_legacy**: Does not have ID fields available for use

Build and Debug
========================

This program is build and include in the FPGA when running the **build_bladeRF.sh** script,
per the [README.md](../../../../../../README.md) in the [bladeRF/hdl/](../../../../../../hdl) directory.

It is also possible to build, load, and debug program via the Eclipse version distributed with Quartus II 15.0.  For JTAG debugging, an Altera USB Blaster or Terasic Blaster is recommended.

(***Linux Users: Ensure you've installed udev rules for your JTAG debugger***)

The below instructions outline this procedure:

- Enter the *NIOS II command shell*.  This is a bash shell that has environment variables required by Quartus already defined.
This is located in: `$QUARTUS_INSTALL_DIR/nios2eds/nios2_command_shell.sh`

- Perform an initial FPGA build to generate the contents of the bladeRF_nios_bsp directory.
```
$ cd $BLADERF_DIR/hdl/quartus

# Replace -s 40 with -s 115 for a bladeRF x115
$ build_bladerf.sh -s 40 -r hosted
```

- Load the FPGA with the resulting image using the bladeRF-cli.  The NIOS II will be loaded and reset in a following step.  This is required to ensure the PCLK from the FX3 is provided to the NIOS II core.

- Launch the Eclipse version provided with Quartus II 15.0, contained located at: `$QUARTUS_INSTALL_DIR/nios2eds/bin/eclipse-nios2`. Create a workspace wherever you see fit.

- Import the bladeRF_nios and bladeRF_nios_bsp projects:
  - From the project explorer, right click and select *Import...*
  - Select *Existing projects into Workspace*
  - Select *Root Directory*
  - Select this directory. You should see both projects checked in the *Projects* pane.
  - Click Finish

- You should now have both projects in your Eclipse workspace. If the C/C++ indexer reports syntax issues due to unknown macro definitions or types, click *Project* -> *C/C++ Index* -> *Rebuild*.

- Connect the JTAG debugger

- Create a debug target:
  - Click *Debug Configurations...*
  - Create a new *NIOS II Hardware v2 (beta)* target
  - For *Project*, select bladeRF_nios
  - For *ELF:*, select bladeRF_nios.elf
  - Under *Connections* -> *Processor*, click *Browse...*.  It should detect the NIOS II via the JTAG device and display a string describing its location in the scan chain.
  - *Optional*  Repeat the above for the *JTAG UART* field, if you wish to use have a console over JTAG. Be careful when using this, as it will significantly affect execution time.
  - Uncheck *Check Unique ID* and *Check timestamp*.
  - If you wish to download code to the device, reset, and run it...
    - Check *Download ELF*, *Reset Processor*, and *Start Processor*
  - If you only wish to inspect the state of the NIOS after observing an issue on the host side...
    - Ensure all of the aforementioned items are unchecked
 - At this point, you may apply the settings and start a debug session.

**Note:** You may have to repeat the step(s) where you browse for the device after unplugging and re-connecting the JTAG debugger.

**Note:** Breaking and stepping though the code will cause libbladeRF to timeout.  To avoid this, configure libbladeRF (via CMake) with -DLIBBLADERF_DISABLE_USB_TIMEOUTS=ON and rebuild libbladeRF.


Host "Simulation" Test Cases
=========================

A substantial portion of the code in this program can be tested on a host machine, using a pre-defined set of test cases.  Run `make -f pcsim.mk` in the [bladeRF_nios/](./bladeRF_nios) directory to build the *bladeRF_nios.sim* program.

Running this program should yield output that prints a test case's description, its request data,
its response data, and "pass."   The test program will abort when it encounters an invalid response,
or if read/write to a simulated device/module contains unexpected information.


NIOS II Core Implementation
=========================

By default, the FPGA build is configured for use with a NIOS II/e implementation, which can be built with the Quartus II Web Edition.

Users with licenses for the NIOS II/s or II/f implementations can pass `-n Small` or `-n Fast` (respectively) to `build_bladerf.sh` to configure the build for the associated NIOS II core. Otherwise, `-n Tiny` is assumed.

For more information, see Altera's [Nios II Core Implementation Details](https://www.altera.com/en_US/pdfs/literature/hb/nios2/n2cpu_nii51015.pdf) document.
