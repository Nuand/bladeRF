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
0x03         | TX synchronization trigger
0x04         | RX synchronization trigger
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

The Nios software is built and embedded into the FPGA when running the **build_bladeRF.sh** script,
per the [README.md](../../../../../../README.md) in the [bladeRF/hdl/](../../../../../../hdl) directory.

It is possible to build, load, and debug the Nios software over JTAG using the Intel/Terasic USB Blaster using either `make` and `gdb`, or Eclipse. The former is quicker to setup, but the latter offers the Eclipse GUI that some may find easier. Both methods are discussed below.

(***Linux Users: Ensure you've installed udev rules for your JTAG debugger***)

Command-Line Debugging with GDB
============================

- Connect the USB Blaster JTAG debugger

- It may be helpful to open several terminal windows/splits for the next few steps.
  - Terminal 1, for:
    - (Re)building the FPGA image
    - Loading the FPGA image
  - Terminal 2, for:
    - (Re)building the Nios
    - Downloading the resulting ELF to the FPGA via the USB Blaster debugger
    - Starting the `nios2-gdb-server` (may use another terminal window for this if desired)
  - Terminal 3, for:
    - Performing the Nios debugging with `nios2-elf-gdb`
  - Terminal 4 (optional), for:
    - Running the host application exercising the bladeRF
  - Terminal 5 (optional), for:
    - Viewing/editing Nios code (helpful for figuring out where to place breakpoints)

- In Terminal 1:
  - Start *NIOS II command shell*. This is a bash shell that has environment variables required by Quartus already defined.
    - `$QUARTUS_INSTALL_DIR/nios2eds/nios2_command_shell.sh`
  - cd into `$BLADERF_DIR/hdl/quartus`
  - Build the FPGA
    ```
    # Replace <board>, <size>, and <rev> according to the platform you are building.
    $ build_bladerf.sh -b <board> -s <size> -r <rev>
    ```
  - Load the FPGA image into the bladeRF device
    ```
    $ bladeRF-cli -l /path/to/hosted.rbf
    ```

- In Terminal 2:
  - Start *NIOS II command shell*.
    - `$QUARTUS_INSTALL_DIR/nios2eds/nios2_command_shell.sh`
  - cd into `$BLADERF_DIR/hdl/fpga/platforms/<platform>/software/bladeRF_nios`
  - For something to do, let's rebuild the Nios. To do this, we must define the `WORKDIR` variable for the make script. This path is relative to the `$BLADERF_DIR/hdl/quartus` directory. Substitute \<platform\> with the bladeRF platform (e.g. bladerf or bladerf-micro), \<size\> with the FPGA size (e.g. 40, 115, A4, A9), and \<rev\> with the revision (e.g. hosted).
    - `make WORKDIR=work/<platform>-<size>-<rev> clean`
    - `make WORKDIR=work/<platform>-<size>-<rev> all`
  - Download the new ELF into the FPGA. Because the FPGA has already been loaded in a previous step, all this does is re-write the appropriate memory space within the FPGA with the new Nios program.
    - `make WORKDIR=work/<platform>-<size>-<rev> download-elf`
  - Finally, start the gdb server
    - `nios2-gdb-server --tcpport 8888 --tcppersist`
      - This may return the error, "Unable to bind (98)". This happens when the previous step still has a use lock on the debugger pod. Give it a minute or two and try again. If it still fails, unplugging the debugger and plugging it back in should do the trick.

- In Terminal 3:
  - Start *NIOS II command shell*.
    - `$QUARTUS_INSTALL_DIR/nios2eds/nios2_command_shell.sh`
  - cd into `$BLADERF_DIR/hdl/quartus/work/<platform>-<size>-<rev>/bladeRF_nios`
  - Start the Nios gdb client:
    - `nios2-elf-gdb -ex "target remote localhost:8888" bladeRF_nios.elf`
  - The Nios processor will now be in a paused state. This lets us setup breakpoints and whatnot as needed for debug. When ready to begin the debug process, issue the `continue` command. This will unpause the Nios.
    - If a breakpoint is hit, gdb will pause the Nios again until you issue another `continue` command. This may cause a timeout and break the user application. Breakpoints can have [commands associated with them](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_34.html), so when one is hit, gdb can immediately print a value and continue. The print operation is slow enough that if more than a few characters are printed, it will trigger a timeout in libbladeRF. Refer to the *Debugging Tips* section for how to increase the timeout.

- In Terminal 4:
  - Run the host software (e.g. bladeRF-cli or some custom program). The general idea is that this software is interacting with the bladeRF/Nios in some way that you want to monitor in gdb in the previous step.

At this point, the Nios code can be edited, recompiled, and downloaded to the FPGA without rebuilding the entire FPGA image. The exception to this is if you are finished debugging and want to embed the changes into the FPGA image, or if the Nios codebase increases such that it no longer fits in the RAM allocated. To rebuild the Nios, quit out of gdb in Terminal 3 and return to Terminal 2. Ctrl-C the `nios2-gdb-server`, rebuild/download the ELF using `make`, and restart `nios2-gdb-server` as described above. Repeat the steps listed in Terminal 3 and 4, above.

Debugging Tips
============================

**Timeouts**

When debugging with gdb, it is often desirable to print the values of variables for any number of reasons. The problem is, the print operation is slow and will cause the host/bladeRF interface to timeout. To make debugging easier, this timeout can be increased.

- Open [host/libraries/libbladeRF/src/backend/usb/usb.h](../../../../../../host/libraries/libbladeRF/src/backend/usb/usb.h)
- Find the line that defines `PERIPHERAL_TIMEOUT_MS` and change it from `250` to a higher value such as `2500` for 2.5 seconds, or `10000` for 10 seconds.
- Save the file and rebuild/install libbladeRF.

**Variable has been optimized out**

Another common occurence when debugging is finding out that a variable has been optimized out and its value is not printable. The best way to get around this is to reduce the level of compiler optimization. This can be done as follows:

- Open [hdl/fpga/platforms/common/bladerf/software/bladeRF_nios/Makefile](../../../../../../hdl/fpga/platforms/common/bladerf/software/bladeRF_nios/Makefile)
- Find the line `APP_CFLAGS_OPTIMIZATION`
- It will have the default optimization for size (`-Os`). Change it to no optimization: `-O0`.
- Save the file.
- Try to rebuild the Nios. It will likely fail due to lack of memory. Somewhere in the output, it should print how much memory is required. Make a note of this.

To increase the Nios RAM size:

- Open `$BLADERF_DIR/hdl/fpga/platforms/<platform>/build/platform.conf`
- Find the `get_qsys_ram` function, and change the return value to be the nearest power of 2 larger than the required memory size from the above. Sometimes simply doubling the existing return value is good enough.
- Rebuild the FPGA in its entirety. This is required because the FPGA needs to instantiate more block RAMs to fit the larger Nios, and it will need to re-fit the surrounding logic.

Eclipse GUI Debugging
=====================

- Connect the USB Blaster JTAG debugger

- Enter the *NIOS II command shell*.  This is a bash shell that has environment variables required by Quartus already defined.
This is located in: `$QUARTUS_INSTALL_DIR/nios2eds/nios2_command_shell.sh`

- Perform an initial FPGA build to generate the contents of the bladeRF_nios_bsp directory.
```
$ cd $BLADERF_DIR/hdl/quartus

# Replace <board>, <size>, and <rev> according to the platform you are building.
$ build_bladerf.sh -b <board> -s <size> -r <rev>
```

- Load the FPGA with the resulting image using the bladeRF-cli.  The NIOS II will be loaded and reset in a following step.  This is required to ensure the PCLK from the FX3 is provided to the NIOS II core.

- Launch the Eclipse version provided with Quartus II 15.0, contained located at: `$QUARTUS_INSTALL_DIR/nios2eds/bin/eclipse-nios2`. Create a workspace wherever you see fit.

- Import the bladeRF_nios_bsp project:
  - From the project explorer, right click and select *Import...*
  - Select *General --> Existing projects into Workspace*
  - Select *Root Directory*
  - Select this directory. You should see a project labeled `bladeRF_nios_bsp` checked in the *Projects* pane.
  - Click Finish

- Import the bladeRF_nios projects:
  - Repeat the previous import steps again, but this time select the software directory for the target platform. For example:
    - `hdl/fpga/platforms/bladerf-micro/software` for bladeRF-micro
    - `hdl/fpga/platforms/bladerf/software` for bladeRF

- You should now have both projects in your Eclipse workspace. If the C/C++ indexer reports syntax issues due to unknown macro definitions or types, click *Project* -> *C/C++ Index* -> *Rebuild*.
  - This doesn't always work and you may have to manually add linked source directories:
    - Right-click the bladeRF_nios project and select *Properties*
    - Under *C/C++ General --> Paths and Symbols*, click the *Source Location* tab
    - Click *Link Folder*
    - Check the box *Link to folder in the file system* and click *Browse...*
    - Navigate to the `bladeRF_nios` folder in this directory
    - If Eclipse complains about the folder name (the field at the top of the window), just change it to `bladeRF_nios_common`
    - Click *OK*
    - Repeat this for the `fpga_common` directory in the bladeRF root.
    - Click *OK* to get back to Eclipse
    - Now Rebuild the C/C++ indexer. You may also have to right-click the project and select *Index --> Search for unresolved includes* or *Index --> Re-resolve unresolved includes*
  - Eclipse may continue to complain about some Altera-specific includes missing, but these can simply be ignored.

- Building the `bladeRF_nios` project will fail stating that the BSP directory could not be found. This is because the environment variable `WORKDIR` has not been set. To set this:
  - Right-click the `bladeRF_nios` project and select *Properties*
  - Under *C/C++ Build --> Environment*, click *Add* to create a new environment variable
  - For *Name*, type `WORKDIR`
  - For *Value*, type `work/<board>-<size>-<rev>` where:
    - `<board>` is the name of the platform (e.g. bladerf or bladerf-micro)
    - `<size>` is the FPGA size (e.g. 40, 115, A4, A9)
    - `<rev>` is the project revision (typically 'hosted')
  - Click *OK*
  - The `bladeRF_nios` project should now build successfully.
  - **Remember to update this variable if targeting a different platform later!**

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
