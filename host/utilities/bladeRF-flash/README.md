# bladeRF-cli: bladeRF Flashing Utility #
bladeRF-flash can be used from the command line to update the FX3 firmware robustly.  Using bladeRF-flash is prefered over ```bladeRF-cli -f``` for flashing the FX3 firmware.

## Dependencies ##
- [libbladeRF][libbladeRF]
- libusbx with hotplug support
 - Linux and Mac hotplug support arrived in 1.0.16
 - Windows hotplug support is still experimental, but functioning.  
   - https://github.com/libusbx/libusbx/issues/9
   - https://github.com/manuelnaranjo/libusbx/tree/windows-hotplug-2
   - https://github.com/litghost/libusbx/tree/windows-hotplug-2

[libbladeRF]: ../../libraries/libbladeRF (libbladeRF)
  
## Basic Usage ##
For usage information, run:

```
bladeRF-flash --help
```

If you encounter issues and ask folks on IRC or in the forum assistance, it's always helpful to note the program and library versions:

```
bladeRF-flash --version
bladeRF-flash --lib-version
```

If only one device is connected, the -d option is not needed. The utility will find and open the attached device. This option is required if multiple devices are connected.

```
bladeRF-flash -f <FX3 firmware image>
```

This will initiate a flash process for the FX3 processor.  If it goes successful you do NOT need to manually reset the board.  For FX3 firmware versions above v1.3 this should go smoothly.

For factory FX3 firmware versions, you may need to manually enter the FX3 bootloader.  See [Forcing Cypress USB bootloader (DFU) mode](http://nuand.com/forums/viewtopic.php?f=6&t=3072).

For factory or v1.2 firmware versions, try reseting the board if bladeRF-flash fails the first time.  The RESET vendor request in these versions is either missing or flaky, so a manual reset may be required.

### Windows notes ###

The first time the bladeRF enters the FX3 bootloader or bladeRF FX3 bootloader (VID 0x1d50:PID 0x6080), you will need to install libusb drivers to allow bladeRF-flash to talk to the bladeRF.  If you get LIBUSB_ERROR_NOT_SUPPORTED, then double check if the libusb drivers are installed for the bladeRF bootloader (VID 0x1d50:PID 0x6080) and FX3 bootloader (VID 04b4:PID 00f3).

### Running from RAM ###

If you are developing FX3 firmware and want to run from RAM only, the -l flag halts the load process once the image is new firmware is running in RAM.

```
bladeRF-flash -l -f <FX3 firmware image>
```

This will leave the SPI flash booting the bladeRF FX3 bootloader.  Combine the -r and -l flags in this state, and you can quickly load new FX3 firmware without flashing to SPI.

```
bladeRF-flash -r -l -f <FX3 firmware image>
```
