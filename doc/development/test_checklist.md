bladeRF Test Checklist
================================================================================

This file serves as a quick summary of tests that should be run regularly by
developers to detect defects and regressions.

Over time these procedures should become automated, and more fine-grained
tests should be introduced. At that time, this document shall be updated to
reflect usage of the automated tests.

When running through the items contained in this file, developers should
maintain an annotated copy of this file, containing:

* The date when tests were run
* The device versions used when performing tests
* "Checks" in the "boxes" ([ ]) and any associated observations or notes

--------------------------------------------------------------------------------

Build Tests
================================================================================

## FPGA Bitstream ##

* Perform FPGA images build with Quartus Prime (free edition) 17.1 (or latest
  version) for the following:

  * Board: bladeRF
    * Revision: hosted
      * [ ] Size: x40
      * [ ] Size: x115

    * Revision: atsc_tx
      * [ ] Size: x40
      * [ ] Size: x115

  * Board: bladeRF-micro
    * Revision: hosted
      * [ ] Size: A2
      * [ ] Size: A4
      * [ ] Size: A9

## FX Firmware ##

* Perform firmware builds for both variants:
  * [ ] Release
  * [ ] Debug

* Ensure the build operates on the FX3 SDKs supported operating systems:
  * [ ] Windows
  * [ ] Linux

* Flash the images to appropriate boards and verify:
  * [ ] Correct version number reported by `bladeRF-cli -e 'version'` (tagged
        builds should have no commit hash; other builds should)
  * [ ] Correct VID/PID and product descriptors are present on `lsusb -v` or
        `dmesg`
    * bladeRF: "Nuand bladeRF" `2cf0:5246`
    * bladeRF Micro: "Nuand bladeRF 2.0" `2cf0:5250`

## Host software ##

Verify that build and installation completes from the top-level, from a clean
build directory, for each of below configurations.

Keep the "treat warnings as errors" option enabled; warnings should be regarded
as defects.

With a device attached, use the following commands to verify the build has
installed and that the desired backend is being used:
```bladeRF-cli -e 'version' -e 'info'```

Additionally, ensure ```libbladeRF_test_c``` and ```libbladeRF_test_cpp```
build and run.

* [ ] Debug build: ```-DCMAKE_BUILD_TYPE=Debug```
* [ ] Release build: ```-DCMAKE_BUILD_TYPE=Release -DTAGGED_RELEASE=Yes```

Perform the above builds with the following:

* Linux (various distributions; see host/misc/docker)
  * This can be performed automatically, using Docker:
    ```bash host/misc/docker/parallel.bash | tee /tmp/output.txt```
  * [ ] GCC
  * [ ] Clang

* macOS (latest)
  * [ ] Apple LLVM

* Windows (currently Windows 10 w/ Visual Studio Community 2017)
  * [ ] Build with support for both libusb and the Cypress backend. This should
        be the default behavior when both present on the build system.
  * [ ] Build with support for each of the two available backends.
  * [ ] Perform builds targeting Win32 and x64

* Misc
  * [ ] Build with documentation (from a clean CMake directory):
        ```-DBUILD_DOCUMENTATION=ON```
    * Any Doxygen warnings should be regarded as defects.
  * [ ] Clang `ccc-analyzer` & `scan-build` (with `-maxloop 100` or greater):
    * CMake incantation:
      ```cmake -DBUILD_DOCUMENTATION=ON -DCMAKE_C_COMPILER=/usr/share/clang/scan-build-6.0/libexec/ccc-analyzer ..```
    * Build and generate report:
      ```/usr/share/clang/scan-build-6.0/bin/scan-build -analyze-headers -maxloop 100 -o ./report make```
    * Any items in the static analysis report should be regarded as defects.
      * Exception: `thirdparty/`


Functional Tests
================================================================================

Most of the functional tests listed here are system-wide integration tests.

Unit tests, Built-in self-tests (BISTs), and top-level test-benches are
very much desirable additions for improving QA and testing efforts.
(Patches are always welcome!)

When appropriate, repeat each test for both USB 2.0 and USB 3.0 connections,
and for both the bladeRF and bladeRF Micro.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## FX3 Firmware Compliance Testing ##

The USB IF Compliance test tools shall be used to identify missing/incorrect
functionality. Run these and record any failures or warnings.

* [USB30CV tool](http://www.usb.org/developers/tools/#usb30tools)
* [USB20CV tool](http://www.usb.org/developers/tools/usb20_tools/#usb20cv)

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## SPI Flash Access ##

The following items are intended to exercise SPI flash accesses.

### OTP region readback ###

There should be no warnings, info, or debug messages reported while performing
this operation.

* [ ] ```bladeRF-cli -e 'info' -v debug``` should report the serial number.

### Calibration region readback ###

There should be no warnings, info, or debug messages reported while performing
this operation.

* [ ] ```bladeRF-cli -e 'info' -v debug``` should report the VCTCXO DAC
      calibration and FPGA size.

### Writing calibration data ###

During the following steps, there should be no error, warning, info, or debug
messages other than the information about flash page read operations.

* [ ] Back up calibration data via:
      ```bladeRF-cli -e 'flash_backup cal.bin cal' -v debug```

* [ ] Write new calibration data (using the appropriate FPGA size) via:
      ```bladeRF-cli -e 'flash_init_cal 40 0x8000' -v debug```

* [ ] Power cycle the device and verify the new values (see the previous
      subsection).

* [ ] Restore the original calibration data via:
      ```bladeRF-cli -e 'flash_restore cal.bin' -v debug```

* [ ] Power cycle the device and verify the new values.

### Firmware Programming ###

Consider any errors or warnings to be defects during these operations.
Info messages are expected, and are intended to provide the user with
feedback about the status of flash operations.

* [ ] Force the device back to the FX3 bootloader via:
    ```bladeRF-cli -e 'jump_to_boot' -v debug```

* [ ] Power cycle the device. It should continue to boot into the bootloader,
      instead of the firmware.

* [ ] "Recover" the device and Flash firmware via the bladeRF-cli:
```
bladeRF> recover <bus> <addr> bladeRF_fw_vX.Y.Z.img
bladeRF> open
bladeRF> load fx3 bladeRF_fw_vX.Y.Z.img
```

* [ ] Power cycle the device and verify that it boots the desired firmware,
      and that the calibration data is intact:
      ```bladeRF-cli -e 'version' -e 'info' -v debug```

* [ ] Flash a different version of firmware via:
      ```bladeRF-cli -f bladeRF_fw_vA.B.C.img -v debug```

* [ ] Power cycle the device and verify that the new firmware has been loaded,
      and that the calibration data is correct.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## FPGA Loading ##

### FPGA Flash-based Autoloading ###

Consider any errors or warnings to be defects during these operations.
Info messages are expected, and are intended to provide the user with
feedback about the status of flash operations.

Ensure that you do not have any FPGA bitstreams on the host set up for
host-based autoloading.

* [ ] Erase any previously stored FPGA images:
      ```bladeRF-cli -L X -v debug```

* [ ] Power cycle the device, and verify that this has not corrupted the
      calibration data or firmware:
      ```bladeRF-cli -e 'version' -e 'info' -v debug```

* [ ] Write an FPGA bitstream to flash:
      ```bladeRF-cli -L <fpga image> -v debug```

* [ ] Power cycle the device and verify that the firmware boots, the FPGA is
      loaded, and that the calibration data has not been corrupted.
      ```bladeRF-cli -e 'version' -e 'info' -v debug```
  * Ensure this works when powered from a USB host, from a USB charger, and
    from a power supply connected to the DC barrel plug.

* [ ] Erase the FPGA and perform the associated verifications.

### Host-based Autoloading ###

* [ ] Place a FPGA image to an autoload directory.

* [ ] Power cycle the device.

* [ ] Verify that the intended FPGA is loaded:
      ```bladeRF-cli -e 'version' -e 'info' -v debug```

### Manual FPGA Loading ###

* [ ] Verify that FPGA loading via ```bladeRF-cli -l <fpga> -v debug```
      succeeds.

* [ ] Verify that the expected version was loaded:
      ```bladeRF-cli -e 'version' -e 'info' -v debug```.

Load a different version with the FPGA already loaded, and preform the
verification step again.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Basic Device Control: bladeRF ##

These device control operations may be performed with the `bladeRF-cli`, or for
simplicity, third party tools, as they should utilize the same underlying
`libbladeRF` API calls.

A signal generator and spectrum analyzer are required for many of these items.

### Frequency Tuning ##

When performing these tests, make notes of the frequencies that you are using.
If issue occur, it is important to know if it is when crossing "bands" or
internal device ranges.

#### RX ####

* [ ] Verify reception of reference signals at various frequencies in
      300 MHz - 3.8 GHz, ***without*** a XB-200 attached.
* [ ] Verify reception of reference signals at various frequencies up to
      3.8 GHz, ***with*** a XB-200 attached, using each filter configuration:
  * [ ] Auto 3 dB filterbank
  * [ ] Auto 1 dB filterbank
  * [ ] 6m filterbank
  * [ ] 2m filterbank
  * [ ] 1.25m filterbank
  * [ ] Custom path

#### TX ####

Suggested reference signal: W-CDMA QPSK test pattern

* [ ] Verify that a VSA can receive and demodulate reference signals at
      frequencies in 300 MHz - 3.8 GHz, ***without*** a XB-200 attached.
* [ ] Verify that a VSA can receive and demodulate reference signals at
      frequencies up to 3.8 GHz, ***with*** a XB-200 attached, using each
      filter configuration:
  * [ ] Auto 3 dB filterbank
  * [ ] Auto 1 dB filterbank
  * [ ] 6m filterbank
  * [ ] 2m filterbank
  * [ ] 1.25m filterbank
  * [ ] Custom path

### Gain ###

#### RX ####

Supply the device with a reference signal and verify the effect of changing
the gain values for:

* [ ] Overall system gain (`set gain rx <dB>`)
* [ ] RX LNA
* [ ] RX VGA1
* [ ] RX VGA2

#### TX ####

Transmit a reference signal to a VSA and verify the effect of changing the
gain values for:

* [ ] Overall system gain (`set gain tx <dB>`)
* [ ] TX VGA1
* [ ] TX VGA2

### Bandwidth ###

#### RX ####

* [ ] Supply a reference signal of a wide bandwith and verify the effect of
      changing the RX bandwidth.

#### TX ####

* [ ] Transmit a reference signal of a wide bandwith to a VSA and verify the
      effect of changing the TX bandwidth.

### DC Offset ###

Repeat the following steps for a variety of frequency/gain combinations and make
note of the results.

* [ ] Use the bladeRF-cli to perform an initial ```cal lms``` at a desired
      frequency and gain.

* [ ] In the CLI, perform a ```cal dc rx``` operation, and note the effect on
      received IQ values and on an FFT plot.

* [ ] Create and load an RX calibration table. Verify that corrections are
      applied over the range of the table.

* [ ] In the CLI, perform a ```cal dc tx``` operation, and transmit a tone into
      a VSA. Make note of the difference in the DC offset.

* [ ] Create and load a TX calibration table. Verify that corrections are
      applied over the range of the table.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Basic Device Control: bladeRF Micro ##

These device control operations may be performed with the `bladeRF-cli`, or for
simplicity, third party tools, as they should utilize the same underlying
`libbladeRF` API calls.

A signal generator and spectrum analyzer are required for many of these items.

### Frequency Tuning ##

When performing these tests, make notes of the frequencies that you are using.
If issue occur, it is important to know if it is when crossing "bands" or
internal device ranges.

#### RX ####

* [ ] Verify reception of reference signals at various frequencies in
      70 MHz - 6 GHz
  * [ ] RX1
  * [ ] RX2
* [ ] Verify that `print rssi` prints reasonable values that vary based on
      signal level.
  * [ ] RX1
  * [ ] RX2

#### TX ####

Suggested reference signal: W-CDMA QPSK test pattern

* [ ] Verify that a VSA can receive and demodulate reference signals at
      frequencies in 70 MHz - 6.0 GHz
  * [ ] TX1
  * [ ] TX2

### Gain ###

#### RX ####

Supply the device with a reference signal and verify the effect of changing
the gain values for:

* [ ] Overall system gain (`set gain rx{1,2} <dB>`)
  * [ ] RX1
  * [ ] RX2
* [ ] `full` stage (`set gain rx{1,2} full <dB>`)
  * [ ] RX1
  * [ ] RX2

#### TX ####

Transmit a reference signal to a VSA and verify the effect of changing the
gain values for:

* [ ] Overall system gain (`set gain tx{1,2} <dB>`)
  * [ ] TX1
  * [ ] TX2
* [ ] `dsa` stage (`set gain tx{1,2} dsa <dB>`)
  * [ ] TX1
  * [ ] TX2

### Bandwidth ###

#### RX ####

* [ ] Supply a reference signal of a wide bandwith and verify the effect of
      changing the RX bandwidth.
  * [ ] RX1
  * [ ] RX2

#### TX ####

* [ ] Transmit a reference signal of a wide bandwith to a VSA and verify the
      effect of changing the TX bandwidth.
  * [ ] TX1
  * [ ] TX2

### ADF4002 ###

Connect a 10 MHz reference source (e.g. GPSDO, or 10 MHz ref output from VSA)
to `REFIN` (J95). The signal should be [TODO: voltage here ... sine/square?]

Note: Ensure the VSA is synchronized to the same clock.

Caution: UFL connectors are fragile and have limited mating cycles.

Transmit a reference signal (e.g. QPSK constellation into EVM analysis, or a
tone into spectrum analysis) into a VSA.

* [ ] With the ADF4002 disabled (`set adf_enable 0`), note the frequency offset
      of the reference signal.
  * [ ] `print adf_enable` should report `ADF Chip Enable: disabled`
* [ ] Enable the ADF4002 (`set adf_enable 1`).
  * [ ] `print adf_enable` should report `ADF Chip Enable: enabled`
        and `ADF Chip Locked: locked`
  * [ ] The frequency offset of the reference signal should be very small,
        within a few Hz.
  * [ ] `print trimdac` should report `0xc000` for current VCTCXO trim
* [ ] Change the ADF4002's reference frequency: `set adf_refclk 9M`
  * [ ] `print adf_enable` should report `ADF Chip Enable: enabled`
        and `ADF Chip Locked: unlocked`
* [ ] Change the ADF4002's reference frequency back to 10 MHz:
      `set adf_refclk 10M`
  * [ ] `print adf_enable` should report `ADF Chip Enable: enabled`
        and `ADF Chip Locked: locked`
* [ ] Disable the reference clock while the ADF is enabled (note: don't
      do this on the UFL connector)
  * [ ] `print adf_enable` should report `ADF Chip Enable: enabled`
        and `ADF Chip Locked: unlocked`
* [ ] Disable the ADF4002 (`set adf_enable 0`)
  * [ ] `print adf_enable` should report `ADF Chip Enable: disabled`
  * [ ] `print trimdac` should NOT report `0xc000` for current VCTCXO trim,
        but should instead return to its previous value.

### Bias tees ###

Note: disconnect RX1/RX2/TX1/TX2 when testing the bias tees to avoid possible
damage to test equipment.

* [ ] Disable the bias tees: `set biastee rx 0 ; set biastee tx 0`
* [ ] Verify 0 VDC between RX1/RX2/TX1/TX2 center pin and ground
* [ ] Enable the RX bias tees: `set biastee rx 1`
* [ ] Verify ~5 VDC between the center pins of RX1 & RX2 and ground, and
      0 VDC between the center pins of TX1 & TX2 and ground
* [ ] Enable the TX bias tees: `set biastee tx 1`
* [ ] Verify ~5 VDC between the center pins of RX1/RX2/TX1/TX2 and ground
* [ ] Disable the RX bias tees: `set biastee rx 0`
* [ ] Verify ~5 VDC between the center pins of TX1 & TX2 and ground, and
      0 VDC between the center pins of RX1 & RX2 and ground
* [ ] Disable the TX bias tees: `set biastee tx 0`
* [ ] Verify 0 VDC between RX1/RX2/TX1/TX2 center pin and ground

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

### libbladeRF test programs ###

The libbladeRF test programs are intended to exercise specific functionalities
provided by the API.  However, these programs inherently exercise device control
functionality not included above. A selection of the ```libbladeRF_test_*```
programs is provided here to attempt to maximize coverage with a small number
of programs.

#### ```libbladeRF_test_ctrl``` ####

This test ensures that various device control calls complete without error.

Run all of its tests on bladeRF1 **without** an XB-200 attached.

* [ ] ```sampling```
* [ ] ```lpf_mode```
* [ ] ```enable_module```
* [ ] ```loopback```
* [ ] ```rx_mux```
* [ ] ```correction```
* [ ] ```samplerate```
* [ ] ```bandwidth```
* [ ] ```gain```
* [ ] ```frequency```
* [ ] ```threads```

Run all of its tests on bladeRF1 **with** an XB-200 attached.

* [ ] ```sampling```
* [ ] ```lpf_mode```
* [ ] ```enable_module```
* [ ] ```loopback```
* [ ] ```rx_mux```
* [ ] ```xb200```
* [ ] ```correction```
* [ ] ```samplerate```
* [ ] ```bandwidth```
* [ ] ```gain```
* [ ] ```frequency```
* [ ] ```threads```

Run all of its tests on bladeRF2 with FPGA-based tuning (default).

* [ ] ```sampling```
* [ ] ```lpf_mode```
* [ ] ```enable_module```
* [ ] ```loopback```
* [ ] ```rx_mux```
* [ ] ```correction```
* [ ] ```samplerate```
* [ ] ```bandwidth```
* [ ] ```gain```
* [ ] ```frequency```
* [ ] ```threads```

Run all of its tests on bladeRF2 with host-based tuning
(`BLADERF_DEFAULT_TUNING_MODE='host'`).

* [ ] ```sampling```
* [ ] ```lpf_mode```
* [ ] ```enable_module```
* [ ] ```loopback```
* [ ] ```rx_mux```
* [ ] ```correction```
* [ ] ```samplerate```
* [ ] ```bandwidth```
* [ ] ```gain```
* [ ] ```frequency```
* [ ] ```threads```

#### ```libbladeRF_test_rx_discont``` ####

* [ ] Run this program over a variety of valid sample rates (for the particular
      USB connection). No discontinuities should occur.

* [ ] Run this program over for sample rates that your USB controller does not
      support. Discontinuities should be reported.

#### ```libbladeRF_test_repeater``` ####

This program uses the async interface to immediately retransmit received
samples.

Supply a reference signal from a signal generator, and verify that it can
be demodulated without failures on a VSA.

Record the sample rates, bandwidths, frequencies, and stream parameters used.

* [ ] Verify that transmitted samples are representative of the received signals.


#### ```libbladeRF_test_timestamps``` ####

This program verifies timestamp functionality. Run all available test at
a variety of sample rates.  Not that some tests (e.g., loopback_onoff) are not
designed to support high sample rates.

* [ ] ```rx_gaps```
* [ ] ```rx_scheduled```
* [ ] ```tx_onoff```
* [ ] ```tx_onoff_nowsched```
* [ ] ```tx_gmsk_bursts```
* [ ] ```loopback_onoff```
* [ ] ```format_mismatch```
* [ ] ```readback```

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

### libbladeRF sample programs ###

As these are intended to exemplify API usage, these programs must be correct.

Verify that the following run as intended:

* [ ] ```libbladeRF_example_open_via_serial```
* [ ] ```libbladeRF_example_sync_rxtx```
* [ ] ```libbladeRF_example_sync_rx_meta```
* [ ] ```libbladeRF_example_sync_tx_meta```


- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Third-party Tools ##

Testing against third-party tools with bladeRF support to ensure changes have
not introduced compatibility issues is highly recommended.

Make note of the tool and its version, for future comparison, should any issues
arise.
