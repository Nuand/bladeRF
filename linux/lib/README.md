# bladeRF Linux Library (libbladeRF) #
The source for the library which interacts with the bladeRF device.  For the time being, the library is linux specific but the API is planned to be able to work in a headless environment where the code is being run on the FX3 or in a NIOS II processor in the FPGA.

This library exposes a set of functions which can:

- Open and close devices
- Tune to different frequencies
- Change the sample rate to an integer or rational rate
- Send and receive complex baseband samples
- Set the bandwidth and gain of the RF chain

The library is meant to be used as a simple and intuitive way to think about not just the bladeRF device itself, but most any radio hardware.  Fundamental operations such as setting the tuning frequency, manipulating switches on the board for different paths, and changing the different transmit and receive gains are all parts of a radio system which need to be exposed at a library level.

## Porting ##
There are only a handful of functions which need to be implemented for a specific platform:

- `lms_spi_read()/write()`
- `si5338_i2c_read()/write()`
- `gpio_read()/gpio_write()`
- `dac_write()`

For linux, these functions perform `ioctl()` calls which send USB packets down to be received by the FPGA over the FX3's serial port, processed, and a result to be sent back up the chain.

For a headless system, these functions would interact with the low level connections either in the FPGA or the FX3 to do exactly what the function intends.

As previously stated, currently the library is meant for linux only but will be extensible to running completely embedded and headless.

## Building ##
1. `make all`
1. `sudo make install`

Currently, the default install prefix is set to `/usr`.  The environment variable `INSTALL_PREFIX` can be used to override this location during the install.

Note that if you do install to a non-standard location, for the uninstall target to work correctly, the same `INSTALL_PREFIX` must be passed as it was to the install target.

To enable debug symbols, a `DEBUG` environment variable can be set.

### Examples ###
Normal make and install:

```
make all && sudo make install
```

Change installation to be `/usr/local` and enable debug symbols:

```
make DEBUG=yes all && sudo make INSTALL_PREFIX=/usr/local all
```

