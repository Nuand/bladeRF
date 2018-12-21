# Copyright (c) 2013-2018 Nuand LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import io
import argparse

from bladerf import _bladerf


__version__ = "1.1.0"


def _bool_to_onoff(val):
    if val is None:
        return "Unsupported"
    elif val:
        return "On"
    else:
        return "Off"


def _strify_list(l):
    return ', '.join([str(x) for x in l])


def _print_channel_details(ch, verbose):
    print("  {}:".format(ch))

    if verbose:
        print("    Is TX        ", ch.is_tx)

    print("    Gain         ", ch.gain)

    if not ch.is_tx:
        print("    Gain Mode    ", ch.gain_mode)
        if verbose:
            print("        Modes    ", _strify_list(ch.gain_modes))

    if not ch.is_tx:
        print("    Symbol RSSI  ", ch.symbol_rssi)

        if verbose:
            print("        Struct   ", repr(ch.rssi))

    print("    Frequency    ", ch.frequency)
    if verbose:
        print("        Range    ", repr(ch.frequency_range))

    print("    Bandwidth    ", ch.bandwidth)
    if verbose:
        print("        Range    ", repr(ch.bandwidth_range))

    print("    Sample Rate  ", ch.sample_rate)
    if verbose:
        print("        Range    ", repr(ch.sample_rate_range))

    if verbose:
        print("    RF Port      ", ch.rf_port)
        print("        Ports    ", _strify_list(ch.rf_ports))
        print("    Bias-T Power ", _bool_to_onoff(ch.bias_tee))


def _print_cmd_info(device=None, devinfo=None, verbose=False):
    b = _bladerf.BladeRF(device, devinfo)

    devinfo = b.get_devinfo()

    flash_size, fs_guessed = b.flash_size

    if verbose:
        print("Object           ", repr(b))
    print("Board Name       ", b.board_name)
    print("Device Speed     ", b.device_speed.name)
    print("FPGA Size        ", b.fpga_size)
    print("FPGA Configured  ", b.fpga_configured)
    if b.is_fpga_configured():
        print("FPGA Version     ", b.fpga_version)
    else:
        print("FPGA Version     ", "Not Configured")
    print("Flash Size       ", (flash_size >> 17), "Mbit",
          "(assumed)" if fs_guessed else "")
    print("Firmware Version ", b.fw_version)

    if verbose:
        print("Clock Select     ", b.clock_select)
        print("Clock Output     ", _bool_to_onoff(b.clock_output))
        print("Power Source     ", b.power_source)
        print("Bus Voltage       {:.3f} V".format(b.bus_voltage or 0))
        print("Bus Current       {:.3f} A".format(b.bus_current or 0))
        print("Bus Power         {:.3f} W".format(b.bus_power or 0))
        print("RFIC temperature  {:.3f} C".format(b.rfic_temperature or 0))

        print("Loopback         ", b.loopback)
        print("    Modes        ", _strify_list(b.loopback_modes))
        print("RX Mux           ", b.rx_mux)

        print("PLL Mode         ", _bool_to_onoff(b.pll_enable))
        if b.pll_enable:
            print("PLL Refclk       ", b.pll_refclk)
            print("PLL Locked       ", _bool_to_onoff(b.pll_locked))

    print("RX Channel Count ", b.rx_channel_count)
    for c in range(b.rx_channel_count):
        ch = b.Channel(_bladerf.CHANNEL_RX(c))
        _print_channel_details(ch, verbose)

    print("TX Channel Count ", b.tx_channel_count)
    for c in range(b.tx_channel_count):
        ch = b.Channel(_bladerf.CHANNEL_TX(c))
        _print_channel_details(ch, verbose)

    b.close()


def cmd_info(device=None, verbose=False, **kwargs):
    if device is None:
        try:
            devinfos = _bladerf.get_device_list()
        except _bladerf.NoDevError:
            print("No bladeRF devices available.")
            return

        print("*** Devices found:", len(devinfos))

        for idx, dev in enumerate(devinfos):
            print()
            print("*** Device", idx)
            _print_cmd_info(devinfo=dev, verbose=verbose)

    else:
        _print_cmd_info(device=device, verbose=verbose)


def cmd_probe(**kwargs):
    try:
        devinfos = _bladerf.get_device_list()
        print("\n".join([str(devinfo) for devinfo in devinfos]))
    except _bladerf.NoDevError:
        print("No bladeRF devices available.")

    try:
        devinfos = _bladerf.get_bootloader_list()
        print("*** Detected one or more FX3-based devices in bootloader mode:")
        print("Bootloader", "\n".join([str(devinfo) for devinfo in devinfos]))
    except _bladerf.NoDevError:
        pass


def cmd_flash_fw(path, device=None, **kwargs):
    b = _bladerf.BladeRF(device)
    b.flash_firmware(path)
    b.close()


def cmd_recover_fw(path, device=None, **kwargs):
    if device is None:
        # Let's guess
        print("No device specified, trying to find one...")
        try:
            devinfos = _bladerf.get_bootloader_list()
        except _bladerf.NoDevError:
            print("No devices in bootloader recovery mode found.")
            return

        if len(devinfos) > 1:
            print("Multiple devices in bootloader recovery mode found.")
            print("Please specify one:")
            print("\n".join([str(devinfo) for devinfo in devinfos]))
            return

        device = "{backend}:device={usb_bus}:{usb_addr}".format(
                    **devinfos[0]._asdict())
        print("Choosing", device)
        print(devinfos[0])

    print("Calling load_fw_from_bootloader...")
    _bladerf.load_fw_from_bootloader(device_identifier=device, file=path)
    print("Complete")


def cmd_load_fpga(path, device=None, **kwargs):
    b = _bladerf.BladeRF(device)
    b.load_fpga(path)
    b.close()


def cmd_flash_fpga(path, device=None, **kwargs):
    b = _bladerf.BladeRF(device)
    b.flash_fpga(path)
    b.close()


def cmd_erase_fpga(device=None, **kwargs):
    b = _bladerf.BladeRF(device)
    b.erase_stored_fpga()
    b.close()


def cmd_rx(outfile, frequency, rate, gain, num_samples, device=None,
           channel=_bladerf.CHANNEL_RX(0), **kwargs):
    b = _bladerf.BladeRF(device)
    ch = b.Channel(channel)

    # Get underlying binary stream for stdout case
    if isinstance(outfile, io.TextIOWrapper):
        outfile = outfile.buffer

    # Configure BladeRF
    ch.frequency = frequency
    ch.sample_rate = rate
    ch.gain = gain

    # Setup synchronous stream
    b.sync_config(layout=_bladerf.ChannelLayout.RX_X1,
                  fmt=_bladerf.Format.SC16_Q11,
                  num_buffers=16,
                  buffer_size=8192,
                  num_transfers=8,
                  stream_timeout=3500)

    # Enable module
    ch.enable = True

    # Create buffer
    bytes_per_sample = 4
    buf = bytearray(1024*bytes_per_sample)
    num_samples_read = 0

    try:
        while True:
            if num_samples > 0 and num_samples_read == num_samples:
                break
            elif num_samples > 0:
                num = min(len(buf)//bytes_per_sample,
                          num_samples-num_samples_read)
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
    ch.enable = False

    b.close()
    outfile.close()


def cmd_tx(infile, frequency, rate, gain, device=None,
           channel=_bladerf.CHANNEL_TX(0), **kwargs):
    b = _bladerf.BladeRF(device)
    ch = b.Channel(channel)

    # Get underlying binary stream for stdin case
    infile = infile.buffer if isinstance(infile, io.TextIOWrapper) else infile

    # Configure BladeRF
    ch.frequency = frequency
    ch.sample_rate = rate
    ch.gain = gain

    # Setup stream
    b.sync_config(layout=_bladerf.ChannelLayout.TX_X1,
                  fmt=_bladerf.Format.SC16_Q11,
                  num_buffers=16,
                  buffer_size=8192,
                  num_transfers=8,
                  stream_timeout=3500)

    # Enable module
    ch.enable = True

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
    ch.enable = False

    b.close()
    infile.close()


def cmd_version(**kwargs):
    print("libbladeRF version:   ", _bladerf.version())
    print("bladerf-tool version: ", "v" + __version__)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='subcommands')

    parser.add_argument('--device', '-d', help='device identifier')

    parser_info = subparsers.add_parser('info', help='get device info')
    parser_info.add_argument('--verbose', '-v', help='print more info',
                             action='store_true')
    parser_info.set_defaults(func=cmd_info)

    parser_probe = subparsers.add_parser('probe', help='probe for devices')
    parser_probe.set_defaults(func=cmd_probe)

    parser_flash_fw = subparsers.add_parser('flash_fw', help='flash firmware')
    parser_flash_fw.add_argument('path', help='firmware image path')
    parser_flash_fw.set_defaults(func=cmd_flash_fw)

    parser_recover_fw = subparsers.add_parser('recover_fw',
                                              help='recover firmware')
    parser_recover_fw.add_argument('path', help='firmware image path')
    parser_recover_fw.set_defaults(func=cmd_recover_fw)

    parser_load_fpga = subparsers.add_parser('load_fpga', help='load fpga')
    parser_load_fpga.add_argument('path', help='fpga image path')
    parser_load_fpga.set_defaults(func=cmd_load_fpga)

    parser_flash_fpga = subparsers.add_parser('flash_fpga', help='flash fpga')
    parser_flash_fpga.add_argument('path', help='fpga image path')
    parser_flash_fpga.set_defaults(func=cmd_flash_fpga)

    parser_erase_fpga = subparsers.add_parser('erase_fpga', help='erase fpga')
    parser_erase_fpga.set_defaults(func=cmd_erase_fpga)

    parser_rx = subparsers.add_parser('rx', help='receive samples',
                                      description='Receive IQ samples. '
                                                  'Sample format is FIXME.')
    parser_rx.add_argument('outfile', metavar='filename',
                           help="IQ samples filename ('-' for stdout)",
                           type=argparse.FileType('wb'))
    parser_rx.add_argument('frequency', help='frequency in Hz', type=float)
    parser_rx.add_argument('rate', help='sample rate in Hz', type=float)
    parser_rx.add_argument('--gain', '-g', help='gain in dB (default 0)',
                           type=float, default=0)
    parser_rx.add_argument('--num-samples', '-n',
                           help='number of samples (default 0 for inf)',
                           type=int, default=0)
    parser_rx.set_defaults(func=cmd_rx)

    parser_tx = subparsers.add_parser('tx', help='transmit samples',
                                      description='Transmit IQ samples. '
                                                  'Sample format is FIXME.')
    parser_tx.add_argument('infile', metavar='filename',
                           help="IQ samples filename ('-' for stdin)",
                           type=argparse.FileType('rb'))
    parser_tx.add_argument('frequency', help='frequency in Hz', type=float)
    parser_tx.add_argument('rate', help='sample rate in Hz', type=float)
    parser_tx.add_argument('--gain', '-g', help='gain in dB (default 0)',
                           type=float, default=0)
    parser_tx.set_defaults(func=cmd_tx)

    parser_version = subparsers.add_parser('version',
                                           help='print version info')
    parser_version.set_defaults(func=cmd_version)

    args = parser.parse_args()
    if 'func' not in args:
        parser.print_help()
        parser.exit()

    args.func(**vars(args))


if __name__ == "__main__":
    main()
