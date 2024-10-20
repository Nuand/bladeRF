# bladeRF-power

## Summary

`bladeRF-power` performs power level measurements using bladeRF devices. This tool provides an interface for users to exercise the gain calibration feature of bladeRF, enabling precise control and adjustment of gain settings. The utility integrates an ncurses GUI for ease of use, allowing users to visually monitor and adjust power levels.

## Install

`bladeRF-power` will be installed as part of the bladeRF host package. To install the host package, follow the instructions in the [host package README](../../../host/README.md).

```bash
cd bladeRF/host/
mkdir ./build && cd ./build
cmake .. && make -j`nproc`
sudo make install
bladeRF-power --help
```

## Loading the Gain Calibration

### Auto Loading (Recommended)

Place the bladeRF's gain calibration file in one of the [auto loading locations](https://github.com/Nuand/bladeRF/wiki/FPGA-Autoloading).

### Manual Loading

If you do not wish to use the auto loading feature, you can manually load the gain calibration file using the `--load` option.

```bash
bladeRF-power --load /path/to/gain_calibration_file.tbl
```

## Troubleshooting

Run bladeRF-power with the example gain calibration within the host build.

> **Note**: The example gain calibration file is not for your device, and will not provide accurate measurements.

```bash
cd bladeRF/host/
mkdir ./build && cd ./build
cmake .. && make -j`nproc`

# Generate example .tbl files for auto loading
./output/libbladeRF_test_gain_calibration

./output/bladeRF-power
```
