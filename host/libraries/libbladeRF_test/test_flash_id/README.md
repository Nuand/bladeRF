# test_flash_id — Flash ID Validation Test

Reads the SPI flash manufacturer ID, device ID, and decoded architecture
from a bladeRF device and validates them against a table of known flash ICs.

## What It Does

1. Open device, read flash architecture from `dev->flash_arch`
2. Print manufacturer ID, device ID, decode status, and geometry
3. Cross-check `bladerf_get_flash_size()` against `flash_arch->tsize_bytes`
4. Validate decode status is `STATUS_SUCCESS`
5. Look up MID/DID in known flash table, validate all architecture fields
6. Run consistency checks (`tsize == psize * num_pages`, etc.)

## Supported Flash ICs

| MID  | DID  | Manufacturer | Device       | Size    |
|------|------|-------------|--------------|---------|
| 0xC2 | 0x36 | Macronix    | MX25U3235E   | 32 Mbit |
| 0xEF | 0x15 | Winbond     | W25Q32JV     | 32 Mbit |
| 0xEF | 0x16 | Winbond     | W25Q64JV     | 64 Mbit |
| 0xEF | 0x17 | Winbond     | W25Q128JV    | 128 Mbit|
| 0x1F | 0x47 | Renesas     | AT25FF321A   | 32 Mbit |

## Usage

```
BLADERF_FORCE_NO_FPGA_PRESENT=1 libbladeRF_test_flash_id [options]
```

The `BLADERF_FORCE_NO_FPGA_PRESENT` variable is only needed when the device
has no calibration data (blank OTP/cal), which prevents FPGA loading.

### Options

| Flag | Description |
|------|-------------|
| `-d, --device <str>` | Device identifier string |
| `-v, --verbosity <l>` | libbladeRF log level |
| `-h, --help` | Show usage text |

## Notes

- This test is read-only and non-destructive.
- Unknown MID/DID combinations result in a test failure but do not affect
  the device.
