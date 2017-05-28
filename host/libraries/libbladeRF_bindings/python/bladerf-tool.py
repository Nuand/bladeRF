import io
import argparse

import bladerf


__version__ = "1.0.0"


def cmd_info(device=None, **kwargs):
    b = bladerf.BladeRF(device)

    devinfo = b.get_devinfo()
    print(devinfo)
    print("")
    print("Board            ", b.get_board_name())
    print("Device Speed     ", b.get_device_speed().name)
    print("FPGA Size        ", b.get_fpga_size())
    print("FPGA Configured  ", b.is_fpga_configured())
    if b.is_fpga_configured():
        print("FPGA Version     ", b.get_fpga_version())
    else:
        print("FPGA Version      Not Configured")
    print("Firmware Version ", b.get_fw_version())
    b.close()


def cmd_probe(**kwargs):
    devinfos = bladerf.get_device_list()
    print("\n".join([str(devinfo) for devinfo in devinfos]))


def cmd_flash_fw(path, device=None, **kwargs):
    b = bladerf.BladeRF(device)
    b.flash_firmware(path)
    b.close()


def cmd_load_fpga(path, device=None, **kwargs):
    b = bladerf.BladeRF(device)
    b.load_fpga(path)
    b.close()


def cmd_flash_fpga(path, device=None, **kwargs):
    b = bladerf.BladeRF(device)
    b.flash_fpga(path)
    b.close()


def cmd_erase_fpga(device=None, **kwargs):
    b = bladerf.BladeRF(device)
    b.erase_fpga(path)
    b.close()


def cmd_rx(outfile, frequency, rate, gain, num_samples, device=None, **kwargs):
    b = bladerf.BladeRF(device)

    # Get underlying binary stream for stdout case
    outfile = outfile.buffer if isinstance(outfile, io.TextIOWrapper) else outfile

    # Configure BladeRF
    b.set_frequency(bladerf.RX, frequency)
    b.set_sample_rate(bladerf.RX, rate)
    b.set_gain(bladerf.RX, gain)

    # Setup synchronous stream
    b.sync_config(bladerf.RX, bladerf.Format.SC16_Q11, 16, 8192, 8, 3500)

    # Enable module
    b.enable_module(bladerf.RX, True)

    # Create buffer
    bytes_per_sample = 4
    buf = bytearray(1024*bytes_per_sample)
    num_samples_read = 0

    try:
        while True:
            if num_samples > 0 and num_samples_read == num_samples:
                break
            elif num_samples > 0:
                num = min(len(buf)//bytes_per_sample, num_samples-num_samples_read)
            else:
                num = len(buf)//bytes_per_sample

            # Read into buffer
            b.sync_rx(buf, num)
            num_samples_read += num

            # Write to file
            outfile.write(buf[:num*bytes_per_sample])
    except KeyboardInterrupt:
        pass

    # Disable module
    b.enable_module(bladerf.RX, False)

    b.close()
    outfile.close()


def cmd_tx(infile, frequency, rate, gain, device=None, **kwargs):
    b = bladerf.BladeRF(device)

    # Get underlying binary stream for stdin case
    infile = infile.buffer if isinstance(infile, io.TextIOWrapper) else infile

    # Configure BladeRF
    b.set_frequency(bladerf.TX, frequency)
    b.set_sample_rate(bladerf.TX, rate)
    b.set_gain(bladerf.TX, gain)

    # Setup stream
    b.sync_config(bladerf.TX, bladerf.Format.SC16_Q11, 16, 8192, 8, 3500)

    # Enable module
    b.enable_module(bladerf.TX, True)

    # Create buffer
    bytes_per_sample = 4
    buf = bytearray(1024*bytes_per_sample)

    try:
        while True:
            # Read samples from file into buf
            num = infile.readinto(buf)
            if num == 0:
                break

            # Convert number of bytes read to samples
            num //= bytes_per_sample

            # Write to bladeRF
            b.sync_tx(buf, num)
    except KeyboardInterrupt:
        pass

    # Disable module
    b.enable_module(bladerf.TX, False)

    b.close()
    infile.close()


def cmd_version(**kwargs):
    print("libbladeRF version:   ", bladerf.version())
    print("bladerf-tool version: ", "v" + __version__)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='subcommands')

    parser.add_argument('--device', '-d', help='device identifier')

    parser_info = subparsers.add_parser('info', help='get device info')
    parser_info.set_defaults(func=cmd_info)

    parser_probe = subparsers.add_parser('probe', help='probe for devices')
    parser_probe.set_defaults(func=cmd_probe)

    parser_flash_fw = subparsers.add_parser('flash_fw', help='flash firmware')
    parser_flash_fw.add_argument('path', help='firmware image path')
    parser_flash_fw.set_defaults(func=cmd_flash_fw)

    parser_load_fpga = subparsers.add_parser('load_fpga', help='load fpga')
    parser_load_fpga.add_argument('path', help='fpga image path')
    parser_load_fpga.set_defaults(func=cmd_load_fpga)

    parser_flash_fpga = subparsers.add_parser('flash_fpga', help='flash fpga')
    parser_flash_fpga.add_argument('path', help='fpga image path')
    parser_flash_fpga.set_defaults(func=cmd_flash_fpga)

    parser_erase_fpga = subparsers.add_parser('erase_fpga', help='erase fpga')
    parser_erase_fpga.set_defaults(func=cmd_erase_fpga)

    parser_rx = subparsers.add_parser('rx', help='receive samples', description='Receive IQ samples. Sample format is FIXME.')
    parser_rx.add_argument('outfile', metavar='filename', help="IQ samples filename ('-' for stdout)", type=argparse.FileType('wb'))
    parser_rx.add_argument('frequency', help='frequency in Hz', type=float)
    parser_rx.add_argument('rate', help='sample rate in Hz', type=float)
    parser_rx.add_argument('--gain', '-g', help='gain in dB (default 0)', type=float, default=0)
    parser_rx.add_argument('--num-samples', '-n', help='number of samples (default 0 for inf)', type=int, default=0)
    parser_rx.set_defaults(func=cmd_rx)

    parser_tx = subparsers.add_parser('tx', help='transmit samples', description='Transmit IQ samples. Sample format is FIXME.')
    parser_tx.add_argument('infile', metavar='filename', help="IQ samples filename ('-' for stdin)", type=argparse.FileType('rb'))
    parser_tx.add_argument('frequency', help='frequency in Hz', type=float)
    parser_tx.add_argument('rate', help='sample rate in Hz', type=float)
    parser_tx.add_argument('--gain', '-g', help='gain in dB (default 0)', type=float, default=0)
    parser_tx.set_defaults(func=cmd_tx)

    parser_version = subparsers.add_parser('version', help='print version info')
    parser_version.set_defaults(func=cmd_version)

    args = parser.parse_args()
    if 'func' not in args:
        parser.print_help()
        parser.exit()

    args.func(**vars(args))
