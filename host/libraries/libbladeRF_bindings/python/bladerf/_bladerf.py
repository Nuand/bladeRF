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


import enum
import collections

import cffi

from sys import platform

from ._cdef import header

ffi = cffi.FFI()

ffi.cdef(header)

if platform == "win32":
      libbladeRF = ffi.dlopen("bladerf.dll")
elif platform == "darwin":
    libbladeRF = ffi.dlopen("libbladeRF.dylib")
else:
    libbladeRF = ffi.dlopen("libbladeRF.so")


###############################################################################


# Python class wrappers for various structures

class ReturnCode(enum.Enum):
    BladeRFError = 0
    UnexpectedError = -1
    RangeError = -2
    InvalError = -3
    MemError = -4
    IOError = -5
    TimeoutError = -6
    NoDevError = -7
    UnsupportedError = -8
    MisalignedError = -9
    ChecksumError = -10
    NoFileError = -11
    UpdateFPGAError = -12
    UpdateFWError = -13
    TimePastError = -14
    QueueFullError = -15
    FPGAOpError = -16
    PermissionError = -17
    WouldBlockError = -18
    NotInitError = -19

    def __str__(self):
        return self.name


class Version(collections.namedtuple("Version", [
                "major", "minor", "patch", "describe"])):
    @staticmethod
    def from_struct(version):
        return Version(version.major, version.minor, version.patch,
                       ffi.string(version.describe).decode())

    def __str__(self):
        return "v{}.{}.{} (\"{}\")".format(*self)


class RationalRate(collections.namedtuple("RationalRate", [
                    "integer", "num", "den"])):
    @staticmethod
    def from_struct(rate):
        return RationalRate(rate.integer, rate.num, rate.den)

    def to_struct(self):
        return ffi.new("struct bladerf_rational_rate *", [
                        self.integer, self.num, self.den])

    struct = property(to_struct)


class Backend(enum.Enum):
    Any = libbladeRF.BLADERF_BACKEND_ANY
    Linux = libbladeRF.BLADERF_BACKEND_LINUX
    LibUSB = libbladeRF.BLADERF_BACKEND_LIBUSB
    Cypress = libbladeRF.BLADERF_BACKEND_CYPRESS
    Dummy = libbladeRF.BLADERF_BACKEND_DUMMY

    def __str__(self):
        return ffi.string(libbladeRF.bladerf_backend_str(self.value)).decode()


class DevInfo(collections.namedtuple("DevInfo", [
                "backend", "serial", "usb_bus", "usb_addr", "instance"])):
    @staticmethod
    def from_struct(devinfo):
        return DevInfo(Backend(devinfo.backend),
                       bytes(ffi.string(devinfo.serial)),
                       devinfo.usb_bus,
                       devinfo.usb_addr,
                       devinfo.instance)

    def to_struct(self):
        return ffi.new("struct bladerf_devinfo *", [
                        Backend(self.backend).value, self.serial, self.usb_bus,
                        self.usb_addr, self.instance])

    struct = property(to_struct)

    @property
    def devstr(self):
        d = self._asdict()
        d['serial'] = self.serial_str
        return ("{backend}:device={usb_bus}:{usb_addr} instance={instance}"
                " serial={serial}".format(**d))

    @property
    def serial_str(self):
        return self.serial.decode()

    def __str__(self):
        return ("Device Information\n" +
                "    backend  {}\n".format(self.backend) +
                "    serial   {}\n".format(self.serial_str) +
                "    usb_bus  {}\n".format(self.usb_bus) +
                "    usb_addr {}\n".format(self.usb_addr) +
                "    instance {}".format(self.instance))

    def __repr__(self):
        return '<DevInfo({})>'.format(self.devstr)


class Range(collections.namedtuple("Range", ["min", "max", "step", "scale"])):
    @staticmethod
    def from_struct(_range):
        return Range(_range.min, _range.max, _range.step, _range.scale)

    def __str__(self):
        return ("Range\n" +
                "    min   {}\n".format(self.min) +
                "    max   {}\n".format(self.max) +
                "    step  {}\n".format(self.step) +
                "    scale {}\n".format(self.scale))

    def __repr__(self):
        return '<Range(min={min},max={max},step={step},scale={scale})>'.format(
            **self._asdict())


class DeviceSpeed(enum.Enum):
    Unknown = libbladeRF.BLADERF_DEVICE_SPEED_UNKNOWN
    High = libbladeRF.BLADERF_DEVICE_SPEED_HIGH
    Super = libbladeRF.BLADERF_DEVICE_SPEED_SUPER


class GainMode(enum.Enum):
    Default = libbladeRF.BLADERF_GAIN_DEFAULT
    Manual = libbladeRF.BLADERF_GAIN_MGC
    FastAttack_AGC = libbladeRF.BLADERF_GAIN_FASTATTACK_AGC
    SlowAttack_AGC = libbladeRF.BLADERF_GAIN_SLOWATTACK_AGC
    Hybrid_AGC = libbladeRF.BLADERF_GAIN_HYBRID_AGC

    def __str__(self):
        return self.name

    def __int__(self):
        return self.value


class Direction(enum.Enum):
    TX = libbladeRF.BLADERF_TX
    RX = libbladeRF.BLADERF_RX


class ChannelLayout(enum.Enum):
    RX_X1 = libbladeRF.BLADERF_RX_X1
    TX_X1 = libbladeRF.BLADERF_TX_X1
    RX_X2 = libbladeRF.BLADERF_RX_X2
    TX_X2 = libbladeRF.BLADERF_TX_X2


class Correction(enum.Enum):
    DCOFF_I = libbladeRF.BLADERF_CORR_DCOFF_I
    DCOFF_Q = libbladeRF.BLADERF_CORR_DCOFF_Q
    PHASE = libbladeRF.BLADERF_CORR_PHASE
    GAIN = libbladeRF.BLADERF_CORR_GAIN


class Format(enum.Enum):
    SC16_Q11 = libbladeRF.BLADERF_FORMAT_SC16_Q11
    SC16_Q11_META = libbladeRF.BLADERF_FORMAT_SC16_Q11_META
    PACKET_META = libbladeRF.BLADERF_FORMAT_PACKET_META
    SC8_Q7 = libbladeRF.BLADERF_FORMAT_SC8_Q7
    SC8_Q7_META = libbladeRF.BLADERF_FORMAT_SC8_Q7_META


class Loopback(enum.Enum):
    Disabled = libbladeRF.BLADERF_LB_NONE
    Firmware = libbladeRF.BLADERF_LB_FIRMWARE
    BB_TXLPF_RXVGA2 = libbladeRF.BLADERF_LB_BB_TXLPF_RXVGA2
    BB_TXVGA1_RXVGA2 = libbladeRF.BLADERF_LB_BB_TXVGA1_RXVGA2
    BB_TXLPF_RXLPF = libbladeRF.BLADERF_LB_BB_TXLPF_RXLPF
    BB_TXVGA1_RXLPF = libbladeRF.BLADERF_LB_BB_TXVGA1_RXLPF
    RF_LNA1 = libbladeRF.BLADERF_LB_RF_LNA1
    RF_LNA2 = libbladeRF.BLADERF_LB_RF_LNA2
    RF_LNA3 = libbladeRF.BLADERF_LB_RF_LNA3
    RFIC_BIST = libbladeRF.BLADERF_LB_RFIC_BIST

    def __str__(self):
        return self.name


class RXMux(enum.Enum):
    Invalid = libbladeRF.BLADERF_RX_MUX_INVALID
    Baseband = libbladeRF.BLADERF_RX_MUX_BASEBAND
    Counter_12bit = libbladeRF.BLADERF_RX_MUX_12BIT_COUNTER
    Counter_32bit = libbladeRF.BLADERF_RX_MUX_32BIT_COUNTER
    Digital_Loopback = libbladeRF.BLADERF_RX_MUX_DIGITAL_LOOPBACK

    def __str__(self):
        return self.name


class ClockSelect(enum.Enum):
    Unknown = -99
    VCTCXO = libbladeRF.CLOCK_SELECT_ONBOARD
    External = libbladeRF.CLOCK_SELECT_EXTERNAL

    def __str__(self):
        return self.name


class RSSI(collections.namedtuple("RSSI", ["preamble", "symbol"])):
    def __str__(self):
        return ("RSSI\n" +
                "    preamble {preamble} dB\n" +
                "    symbol   {symbol} dB\n").format(**self._asdict())

    def __repr__(self):
        return ("<RSSI(preamble={preamble}," +
                "symbol={symbol})>").format(**self._asdict())


class PowerSource(enum.Enum):
    Unknown = libbladeRF.BLADERF_UNKNOWN
    DC_Barrel = libbladeRF.BLADERF_PS_DC
    USB_VBUS = libbladeRF.BLADERF_PS_USB_VBUS

    def __str__(self):
        return self.name


class PMICRegister(enum.Enum):
    Configuration = libbladeRF.BLADERF_PMIC_CONFIGURATION
    Voltage_shunt = libbladeRF.BLADERF_PMIC_VOLTAGE_SHUNT
    Voltage_bus = libbladeRF.BLADERF_PMIC_VOLTAGE_BUS
    Power = libbladeRF.BLADERF_PMIC_POWER
    Current = libbladeRF.BLADERF_PMIC_CURRENT
    Calibration = libbladeRF.BLADERF_PMIC_CALIBRATION

    @property
    def ctype(self):
        """C type definition for the pointer to the returned value."""
        if self in [self.Configuration, self.Calibration]:
            return "uint16_t *"
        else:
            return "float *"


class Serial(collections.namedtuple("Serial", ["serial"])):
    @staticmethod
    def from_struct(serial):
        return Serial(ffi.string(serial.serial).decode())

    def __str__(self):
        return self.serial


###############################################################################


class BladeRFError(Exception):
    """A general libbladeRF exception occurred"""
    def __init__(self, code=0, msg=None):
        if isinstance(code, str):
            # SubclassedError("user message")
            msg = code
            code = 0

        if code == 0:
            i_am = self.__class__.__name__
            code = getattr(ReturnCode, i_am).value

        self.errstr = ffi.string(libbladeRF.bladerf_strerror(code)).decode()

        if msg is not None:
            self.errstr += " (" + msg + ")"

    def __str__(self):
        return self.errstr


class UnexpectedError(BladeRFError):
    """An unexpected failure occurred"""


class RangeError(BladeRFError):
    """Provided parameter is out of range"""


class InvalError(BladeRFError):
    """Invalid operation/parameter"""


class MemError(BladeRFError):
    """Memory allocation error"""


class IOError(BladeRFError):
    """File/Device I/O error"""


class TimeoutError(BladeRFError):
    """Operation timed out"""


class NoDevError(BladeRFError):
    """No device(s) available"""


class UnsupportedError(BladeRFError):
    """Operation not supported"""


class MisalignedError(BladeRFError):
    """Misaligned flash access"""


class ChecksumError(BladeRFError):
    """Invalid checksum"""


class NoFileError(BladeRFError):
    """File not found"""


class UpdateFPGAError(BladeRFError):
    """An FPGA update is required"""


class UpdateFWError(BladeRFError):
    """A firmware update is required"""


class TimePastError(BladeRFError):
    """Requested timestamp is in the past"""


class QueueFullError(BladeRFError):
    """Could not enqueue data into full queue"""


class FPGAOpError(BladeRFError):
    """An FPGA operation reported failure"""


class PermissionError(BladeRFError):
    """Insufficient permissions for the requested operation"""


class WouldBlockError(BladeRFError):
    """Operation would block, but has been requested to be non-blocking."""


class NotInitError(BladeRFError):
    """Device insufficiently initialized for operation"""


def _check_error(err):
    """If the result code indicates an error, raise an exception"""
    if err < 0:
        try:
            errorval = ReturnCode(err)
        except ValueError:
            errorval = ReturnCode(0)

        raise globals()[errorval.name](err)


###############################################################################


def set_verbosity(level):
    libbladeRF.bladerf_log_set_verbosity(level)


def version():
    version = ffi.new("struct bladerf_version *")
    libbladeRF.bladerf_version(version)
    return Version.from_struct(version)


def get_device_list():
    devices = ffi.new("struct bladerf_devinfo *[1]")
    ret = libbladeRF.bladerf_get_device_list(devices)
    _check_error(ret)
    devinfos = [DevInfo.from_struct(devices[0][i]) for i in range(ret)]
    libbladeRF.bladerf_free_device_list(devices[0])
    return devinfos


def get_bootloader_list():
    devices = ffi.new("struct bladerf_devinfo *[1]")
    ret = libbladeRF.bladerf_get_bootloader_list(devices)
    _check_error(ret)
    devinfos = [DevInfo.from_struct(devices[0][i]) for i in range(ret)]
    libbladeRF.bladerf_free_device_list(devices[0])
    return devinfos


def load_fw_from_bootloader(device_identifier=None, backend=0, bus=0, addr=0,
                            file=None):
    ret = libbladeRF.bladerf_load_fw_from_bootloader(
        device_identifier.encode(), backend, bus, addr, file.encode())
    _check_error(ret)


###############################################################################


RX = 0x0
TX = 0x1


def CHANNEL_RX(ch):
    return (ch << 1) | RX


def CHANNEL_TX(ch):
    return (ch << 1) | TX


# defaults for PLL on bladeRF 2
BLADERF_VCTCXO_FREQUENCY = 38.4e6
BLADERF_REFIN_DEFAULT = 10.0e6

BLADERF_SERIAL_LENGTH = 33


###############################################################################


class BladeRF:
    """Python class for interacting with bladeRF boards.

    Example usage:

    >>> d = bladerf.BladeRF()
    >>> d.board_name
    'bladerf1'
    """

    def __init__(self, device_identifier=None, devinfo=None):
        self.dev = ffi.new("struct bladerf *[1]")
        self.open(device_identifier, devinfo)

    def __repr__(self):
        return '<BladeRF({!r})>'.format(self.devinfo)

    # Open, close, devinfo

    def open(self, device_identifier=None, devinfo=None):
        if devinfo is not None:
            ret = libbladeRF.bladerf_open_with_devinfo(self.dev,
                                                       devinfo.struct)
        elif device_identifier is not None:
            ret = libbladeRF.bladerf_open(self.dev, device_identifier.encode())
        else:
            ret = libbladeRF.bladerf_open(self.dev, ffi.NULL)
        _check_error(ret)

    def close(self):
        libbladeRF.bladerf_close(self.dev[0])

    def get_devinfo(self):
        devinfo = ffi.new("struct bladerf_devinfo *")
        ret = libbladeRF.bladerf_get_devinfo(self.dev[0], devinfo)
        _check_error(ret)
        return DevInfo.from_struct(devinfo)

    devinfo = property(get_devinfo,
                       doc="Device information as a DevInfo structure")

    # Device properties

    def get_device_speed(self):
        return DeviceSpeed(libbladeRF.bladerf_device_speed(self.dev[0]))

    device_speed = property(get_device_speed,
                            doc="USB speed of the open device")

    def get_serial_struct(self):
        serial = ffi.new("struct bladerf_serial *")
        ret = libbladeRF.bladerf_get_serial_struct(self.dev[0], serial)
        _check_error(ret)
        return Serial.from_struct(serial)

    def get_serial(self):
        sstruct = self.get_serial_struct()
        return sstruct.serial

    serial = property(get_serial, doc="Serial number of the open device")

    def get_fpga_size(self):
        fpga_size = ffi.new("bladerf_fpga_size *")
        ret = libbladeRF.bladerf_get_fpga_size(self.dev[0], fpga_size)
        _check_error(ret)
        return fpga_size[0]

    fpga_size = property(get_fpga_size, doc="FPGA size in kLE")

    def get_flash_size(self):
        flash_size = ffi.new("uint32_t *")
        is_guess = ffi.new("bool *")
        ret = libbladeRF.bladerf_get_flash_size(self.dev[0], flash_size,
                                                is_guess)
        _check_error(ret)
        return (flash_size[0], is_guess[0])

    flash_size = property(get_flash_size, doc="Flash size in bytes")

    def is_fpga_configured(self):
        ret = libbladeRF.bladerf_is_fpga_configured(self.dev[0])
        _check_error(ret)
        return bool(ret)

    fpga_configured = property(is_fpga_configured,
                               doc="True if the device's FPGA is configured")

    def get_fpga_version(self):
        version = ffi.new("struct bladerf_version *")
        ret = libbladeRF.bladerf_fpga_version(self.dev[0], version)
        _check_error(ret)
        return Version.from_struct(version)

    fpga_version = property(get_fpga_version, doc="FPGA version information")

    def get_fw_version(self):
        version = ffi.new("struct bladerf_version *")
        ret = libbladeRF.bladerf_fw_version(self.dev[0], version)
        _check_error(ret)
        return Version.from_struct(version)

    fw_version = property(get_fw_version, doc="Firmware version information")

    def get_board_name(self):
        board_name = libbladeRF.bladerf_get_board_name(self.dev[0])
        return ffi.string(board_name).decode()

    board_name = property(get_board_name,
                          doc="The board model name, as a string")

    def trim_dac_write(self, val):
        libbladeRF.bladerf_trim_dac_write(self.dev[0], val)

    def trim_dac_read(self):
        try:
            trim_val = ffi.new("uint16_t *")
            ret = libbladeRF.bladerf_trim_dac_read(self.dev[0], trim_val)
            _check_error(ret)
            return trim_val[0]
        except UnsupportedError:
            return None

    def get_channel_count(self, direction):
        return libbladeRF.bladerf_get_channel_count(self.dev[0], direction)

    @property
    def rx_channel_count(self):
        """Number of RX channels on the device"""
        return self.get_channel_count(libbladeRF.BLADERF_RX)

    @property
    def tx_channel_count(self):
        """Number of TX channels on the device"""
        return self.get_channel_count(libbladeRF.BLADERF_TX)

    # Enable/Disable

    def enable_module(self, ch, enable):
        ret = libbladeRF.bladerf_enable_module(self.dev[0], ch, bool(enable))
        _check_error(ret)

    # Gain

    def set_gain(self, ch, gain):
        ret = libbladeRF.bladerf_set_gain(self.dev[0], ch, gain)
        _check_error(ret)

    def get_gain(self, ch):
        gain = ffi.new("bladerf_gain *")
        ret = libbladeRF.bladerf_get_gain(self.dev[0], ch, gain)
        _check_error(ret)
        return gain[0]

    def set_gain_mode(self, ch, mode):
        ret = libbladeRF.bladerf_set_gain_mode(self.dev[0], ch, int(mode))
        _check_error(ret)

    def get_gain_mode(self, ch):
        try:
            mode = ffi.new("bladerf_gain_mode *")
            ret = libbladeRF.bladerf_get_gain_mode(self.dev[0], ch, mode)
            _check_error(ret)
            return GainMode(mode[0])
        except UnsupportedError:
            return None

    def get_gain_modes(self, ch):
        modes_arr = ffi.new("struct bladerf_gain_modes **")
        ret = libbladeRF.bladerf_get_gain_modes(self.dev[0], ch, modes_arr)
        _check_error(ret)
        return [GainMode(modes_arr[0][i].mode) for i in range(ret)]

    def get_gain_range(self, ch):
        _range_ptr = ffi.new("struct bladerf_range **")
        ret = libbladeRF.bladerf_get_gain_range(self.dev[0], ch, _range_ptr)
        _check_error(ret)
        return Range.from_struct(_range_ptr[0])

    def set_gain_stage(self, ch, stage, gain):
        ret = libbladeRF.bladerf_set_gain_stage(self.dev[0], ch,
                                                stage.encode(), gain)
        _check_error(ret)

    def get_gain_stage(self, ch, stage):
        gain = ffi.new("bladerf_gain *")
        ret = libbladeRF.bladerf_get_gain_stage(self.dev[0], ch,
                                                stage.encode(), gain)
        _check_error(ret)
        return gain[0]

    def get_gain_stage_range(self, ch, stage):
        _range_ptr = ffi.new("struct bladerf_range **")
        ret = libbladeRF.bladerf_get_gain_stage_range(self.dev[0], ch,
                                                      stage.encode(),
                                                      _range_ptr)
        _check_error(ret)
        return Range.from_struct(_range_ptr[0])

    def get_gain_stages(self, ch):
        ret = libbladeRF.bladerf_get_gain_stages(self.dev[0], ch, ffi.NULL, 0)
        _check_error(ret)
        stages_arr = ffi.new("const char *[]", ret)
        ret = libbladeRF.bladerf_get_gain_stages(self.dev[0], ch, stages_arr,
                                                 ret)
        _check_error(ret)
        return [ffi.string(stages_arr[i]).decode() for i in range(ret)]

    # Sample rate

    def set_sample_rate(self, ch, rate):
        actual_rate = ffi.new("bladerf_sample_rate *")
        ret = libbladeRF.bladerf_set_sample_rate(self.dev[0], ch, int(rate),
                                                 actual_rate)
        _check_error(ret)
        return actual_rate[0]

    def set_rational_sample_rate(self, ch, rational_rate):
        raise NotImplementedError()

    def get_sample_rate(self, ch):
        rate = ffi.new("bladerf_sample_rate *")
        ret = libbladeRF.bladerf_get_sample_rate(self.dev[0], ch, rate)
        _check_error(ret)
        return rate[0]

    def get_rational_sample_rate(self, ch):
        raise NotImplementedError()

    def get_sample_rate_range(self, ch):
        _range_ptr = ffi.new("struct bladerf_range **")
        ret = libbladeRF.bladerf_get_sample_rate_range(self.dev[0], ch,
                                                       _range_ptr)
        _check_error(ret)
        return Range.from_struct(_range_ptr[0])

    # Bandwidth

    def set_bandwidth(self, ch, bandwidth):
        actual_bandwidth = ffi.new("bladerf_bandwidth *")
        ret = libbladeRF.bladerf_set_bandwidth(self.dev[0], ch, int(bandwidth),
                                               actual_bandwidth)
        _check_error(ret)
        return actual_bandwidth[0]

    def get_bandwidth(self, ch):
        bandwidth = ffi.new("bladerf_bandwidth *")
        ret = libbladeRF.bladerf_get_bandwidth(self.dev[0], ch, bandwidth)
        _check_error(ret)
        return bandwidth[0]

    def get_bandwidth_range(self, ch):
        _range_ptr = ffi.new("struct bladerf_range **")
        ret = libbladeRF.bladerf_get_bandwidth_range(self.dev[0], ch,
                                                     _range_ptr)
        _check_error(ret)
        return Range.from_struct(_range_ptr[0])

    # Frequency

    def set_frequency(self, ch, frequency):
        ret = libbladeRF.bladerf_set_frequency(self.dev[0], ch, int(frequency))
        _check_error(ret)

    def get_frequency(self, ch):
        frequency = ffi.new("bladerf_frequency *")
        ret = libbladeRF.bladerf_get_frequency(self.dev[0], ch, frequency)
        _check_error(ret)
        return frequency[0]

    def select_band(self, ch, frequency):
        ret = libbladeRF.bladerf_select_band(self.dev[0], ch, int(frequency))
        _check_error(ret)

    def get_frequency_range(self, ch):
        _range_ptr = ffi.new("struct bladerf_range **")
        ret = libbladeRF.bladerf_get_frequency_range(self.dev[0], ch,
                                                     _range_ptr)
        _check_error(ret)
        return Range.from_struct(_range_ptr[0])

    # RF Ports

    def set_rf_port(self, ch, port):
        ret = libbladeRF.bladerf_set_rf_port(self.dev[0], ch, port.encode())
        _check_error(ret)

    def get_rf_port(self, ch):
        try:
            port = ffi.new("const char *[1]")
            ret = libbladeRF.bladerf_get_rf_port(self.dev[0], ch, port)
            _check_error(ret)
            return ffi.string(port[0]).decode()
        except UnsupportedError:
            return None

    def get_rf_ports(self, ch):
        try:
            count = libbladeRF.bladerf_get_rf_ports(self.dev[0], ch, ffi.NULL,
                                                    0)
            _check_error(count)
            ports_arr = ffi.new("const char *[]", count)
            ret = libbladeRF.bladerf_get_rf_ports(self.dev[0], ch, ports_arr,
                                                  count)
            _check_error(ret)
            return [ffi.string(ports_arr[i]).decode() for i in range(ret)]
        except UnsupportedError:
            return []

    # Sample Loopback

    def get_loopback_modes(self):
        ret = libbladeRF.bladerf_get_loopback_modes(self.dev[0], ffi.NULL)
        _check_error(ret)
        modes_arr = ffi.new("struct bladerf_loopback_modes *[]", ret)
        ret = libbladeRF.bladerf_get_loopback_modes(self.dev[0], modes_arr)
        _check_error(ret)
        return [Loopback(modes_arr[0][i].mode) for i in range(ret)]

    loopback_modes = property(get_loopback_modes,
                              doc="Supported loopback modes")

    def is_loopback_mode_supported(self, lb):
        if isinstance(lb, Loopback):
            lb = lb.value
        ret = libbladeRF.bladerf_is_loopback_mode_supported(self.dev[0], lb)
        _check_error(ret)
        return bool(ret)

    def get_loopback(self):
        lb = ffi.new("bladerf_loopback *")
        ret = libbladeRF.bladerf_get_loopback(self.dev[0], lb)
        _check_error(ret)
        return Loopback(lb[0])

    def set_loopback(self, lb):
        if isinstance(lb, Loopback):
            lb = lb.value
        ret = libbladeRF.bladerf_set_loopback(self.dev[0], lb)
        _check_error(ret)

    loopback = property(get_loopback, set_loopback, doc="Loopback selection")

    # Trigger TBD

    # Sample RX Mux

    def get_rx_mux(self):
        mux = ffi.new("bladerf_rx_mux *")
        ret = libbladeRF.bladerf_get_rx_mux(self.dev[0], mux)
        _check_error(ret)
        return RXMux(mux[0])

    def set_rx_mux(self, rx_mux):
        if isinstance(rx_mux, RXMux):
            rx_mux = rx_mux.value
        ret = libbladeRF.bladerf_set_rx_mux(self.dev[0], rx_mux)
        _check_error(ret)

    rx_mux = property(get_rx_mux, set_rx_mux, doc="RX Multiplexer selection")

    # DC/Phase/Gain Correction

    def get_correction(self, ch, corr):
        value = ffi.new("bladerf_correction_value *")
        ret = libbladeRF.bladerf_get_correction(self.dev[0], ch, corr.value,
                                                value)
        _check_error(ret)
        return value[0]

    def set_correction(self, ch, corr, value):
        ret = libbladeRF.bladerf_set_correction(self.dev[0], ch, corr.value,
                                                value)
        _check_error(ret)

    # Streaming format

    def interleave_stream_buffer(self, layout, format, buffer_size, samples):
        raise NotImplementedError()

    def deinterleave_stream_buffer(self, layout, format, buffer_size, samples):
        raise NotImplementedError()

    # Streaming

    def sync_config(self, layout, fmt, num_buffers, buffer_size, num_transfers,
                    stream_timeout):
        ret = libbladeRF.bladerf_sync_config(self.dev[0],
                                             layout.value,
                                             fmt.value,
                                             num_buffers,
                                             buffer_size,
                                             num_transfers,
                                             stream_timeout)
        _check_error(ret)

    def sync_tx(self, buf, num_samples, timeout_ms=None, meta=ffi.NULL):
        ret = libbladeRF.bladerf_sync_tx(self.dev[0],
                                         ffi.from_buffer(buf),
                                         num_samples,
                                         ffi.cast("struct bladerf_metadata *", meta),
                                         timeout_ms or 0)
        _check_error(ret)

    def sync_rx(self, buf, num_samples, timeout_ms=None, meta=ffi.NULL):
        ret = libbladeRF.bladerf_sync_rx(self.dev[0],
                                         ffi.from_buffer(buf),
                                         num_samples,
                                         meta,
                                         timeout_ms or 0)
        _check_error(ret)

    # FPGA/Firmware Loading/Flashing

    def load_fpga(self, image_path):
        ret = libbladeRF.bladerf_load_fpga(self.dev[0], image_path.encode())
        _check_error(ret)

    def flash_fpga(self, image_path):
        ret = libbladeRF.bladerf_flash_fpga(self.dev[0], image_path.encode())
        _check_error(ret)

    def erase_stored_fpga(self):
        ret = libbladeRF.bladerf_erase_stored_fpga(self.dev[0])
        _check_error(ret)

    def flash_firmware(self, image_path):
        ret = libbladeRF.bladerf_flash_firmware(self.dev[0],
                                                image_path.encode())
        _check_error(ret)

    def device_reset(self):
        ret = libbladeRF.bladerf_device_reset(self.dev[0])
        _check_error(ret)

    # VCTCXO Tamer Mode

    def set_vctcxo_tamer_mode(self, mode):
        raise NotImplementedError()

    def get_vctcxo_tamer_mode(self):
        raise NotImplementedError()

    vctcxo_tamer_mode = property(get_vctcxo_tamer_mode, set_vctcxo_tamer_mode)

    def get_vctcxo_trim(self):
        raise NotImplementedError()

    vctcxo = property(get_vctcxo_trim)

    # Tuning Mode

    def set_tuning_mode(self, mode):
        raise NotImplementedError()

    def get_tuning_mode(self):
        raise NotImplementedError()

    tuning_mode = property(get_tuning_mode, set_tuning_mode)

    # Bias tee

    def get_bias_tee(self, ch):
        try:
            state = ffi.new("bool *")
            ret = libbladeRF.bladerf_get_bias_tee(self.dev[0], ch, state)
            _check_error(ret)
            return bool(state[0])
        except UnsupportedError:
            return None

    def set_bias_tee(self, ch, enable):
        ret = libbladeRF.bladerf_set_bias_tee(self.dev[0], ch, enable)
        _check_error(ret)

    # RFIC

    def get_rfic_temperature(self):
        try:
            val = ffi.new("float *")
            ret = libbladeRF.bladerf_get_rfic_temperature(self.dev[0], val)
            _check_error(ret)
            return val[0]
        except UnsupportedError:
            return None

    rfic_temperature = property(get_rfic_temperature,
                                doc="RFIC internal temperature")

    def get_rfic_rssi(self, ch):
        try:
            preamble = ffi.new("int32_t *")
            symbol = ffi.new("int32_t *")
            ret = libbladeRF.bladerf_get_rfic_rssi(self.dev[0], ch,
                                                   preamble, symbol)
            _check_error(ret)
            return RSSI(preamble[0], symbol[0])
        except UnsupportedError:
            return None

    def get_rfic_ctrl_out(self):
        try:
            ctrl_out = ffi.new("uint8_t *")
            ret = libbladeRF.bladerf_get_rfic_ctrl_out(self.dev[0], ctrl_out)
            _check_error(ret)
            return ctrl_out[0]
        except UnsupportedError:
            return None

    rfic_ctrl_out = property(get_rfic_ctrl_out,
                             doc="RFIC CTRL_OUT status pins")

    # Phase Detector/Frequency Synthesizer

    def get_pll_lock_state(self):
        try:
            locked = ffi.new("bool *")
            ret = libbladeRF.bladerf_get_pll_lock_state(self.dev[0], locked)
            _check_error(ret)
            return bool(locked[0])
        except UnsupportedError:
            return None

    pll_locked = property(get_pll_lock_state,
                          doc="PLL lock indication")

    def get_pll_enable(self):
        try:
            enable = ffi.new("bool *")
            ret = libbladeRF.bladerf_get_pll_enable(self.dev[0], enable)
            _check_error(ret)
            return bool(enable[0])
        except UnsupportedError:
            return None

    def set_pll_enable(self, enable):
        ret = libbladeRF.bladerf_set_pll_enable(self.dev[0], enable)
        _check_error(ret)

    pll_enable = property(get_pll_enable, set_pll_enable,
                          doc="PLL Enable")

    def get_pll_refclk(self):
        try:
            freq = ffi.new("uint64_t *")
            ret = libbladeRF.bladerf_get_pll_refclk(self.dev[0], freq)
            _check_error(ret)
            return int(freq[0])
        except UnsupportedError:
            return None

    def set_pll_refclk(self, freq):
        ret = libbladeRF.bladerf_set_pll_refclk(self.dev[0], freq)
        _check_error(ret)

    pll_refclk = property(get_pll_refclk, set_pll_refclk,
                          doc="PLL Reference Clock frequency")

    # Power source

    def get_power_source(self):
        try:
            val = ffi.new("bladerf_power_sources *")
            ret = libbladeRF.bladerf_get_power_source(self.dev[0], val)
            _check_error(ret)
            return PowerSource(val[0])
        except UnsupportedError:
            return PowerSource.Unknown

    power_source = property(get_power_source,
                            doc="Active power source for device")

    # Clock select

    def get_clock_select(self):
        try:
            sel = ffi.new("bladerf_clock_select *")
            ret = libbladeRF.bladerf_get_clock_select(self.dev[0], sel)
            _check_error(ret)
            return ClockSelect(sel[0])
        except UnsupportedError:
            return ClockSelect.Unknown

    def set_clock_select(self, sel):
        if isinstance(sel, ClockSelect):
            sel = sel.value
        ret = libbladeRF.bladerf_set_clock_select(self.dev[0], sel)
        _check_error(ret)

    clock_select = property(get_clock_select, set_clock_select,
                            doc="System clock input selection")

    # Clock output

    def get_clock_output(self):
        try:
            state = ffi.new("bool *")
            ret = libbladeRF.bladerf_get_clock_output(self.dev[0], state)
            _check_error(ret)
            return bool(state[0])
        except UnsupportedError:
            return None

    def set_clock_output(self, enable):
        ret = libbladeRF.bladerf_set_clock_output(self.dev[0], enable)
        _check_error(ret)

    clock_output = property(get_clock_output, set_clock_output,
                            doc="Clock output enable")

    # Current/power monitor IC

    def get_pmic_register(self, reg):
        try:
            if not isinstance(reg, PMICRegister):
                reg = PMICRegister(reg)
            val = ffi.new(reg.ctype)
            ret = libbladeRF.bladerf_get_pmic_register(self.dev[0], reg.value,
                                                       val)
            _check_error(ret)
            return val[0]
        except UnsupportedError:
            return None

    @property
    def bus_voltage(self):
        """Main power bus voltage, in Volts"""
        return self.get_pmic_register(PMICRegister.Voltage_bus)

    @property
    def bus_current(self):
        """Main power bus current, in Amps"""
        return self.get_pmic_register(PMICRegister.Current)

    @property
    def bus_power(self):
        """Main power bus power, in Watts"""
        return self.get_pmic_register(PMICRegister.Power)

    # Convenience objects

    class _Channel:
        """Container for bladeRF channel properties"""

        def __init__(self, dev, channel):
            self.dev = dev
            self.channel = channel

            if (channel >> 1) not in range(dev.get_channel_count(self.is_tx)):
                raise UnsupportedError("Invalid channel: " + str(channel))

        def __str__(self):
            return "Channel {}{}".format(
                "TX" if self.is_tx else "RX", (self.channel >> 1) + 1)

        def __repr__(self):
            return '<Channel({},CHANNEL_{}({}))>'.format(
                repr(self.dev), "TX" if self.is_tx else "RX",
                self.channel >> 1)

        @property
        def is_tx(self):
            """Returns True if this is a TX channel, False otherwise"""
            return bool(self.channel & TX)

        @property
        def gain(self):
            """Gain, in dB"""
            return self.dev.get_gain(self.channel)

        @gain.setter
        def gain(self, value):
            return self.dev.set_gain(self.channel, value)

        @property
        def gain_mode(self):
            """Gain mode"""
            return self.dev.get_gain_mode(self.channel)

        @gain_mode.setter
        def gain_mode(self, value):
            return self.dev.set_gain_mode(self.channel, value)

        @property
        def gain_modes(self):
            """List of supported gain modes"""
            return self.dev.get_gain_modes(self.channel)

        @property
        def rssi(self):
            """RSSI report from RFIC"""
            return self.dev.get_rfic_rssi(self.channel)

        @property
        def preamble_rssi(self):
            """Preamble RSSI reading (latched at algorithm reset)"""
            if self.rssi is not None:
                return self.rssi.preamble
            else:
                return None

        @property
        def symbol_rssi(self):
            """Symbol RSSI reading (most recent value)"""
            if self.rssi is not None:
                return self.rssi.symbol
            else:
                return None

        @property
        def sample_rate(self):
            """Sample rate, in samples/second"""
            return self.dev.get_sample_rate(self.channel)

        @sample_rate.setter
        def sample_rate(self, value):
            return self.dev.set_sample_rate(self.channel, value)

        @property
        def sample_rate_range(self):
            """Range of sample rates supported by this channel"""
            return self.dev.get_sample_rate_range(self.channel)

        @property
        def bandwidth(self):
            """Channel bandwidth in Hz"""
            return self.dev.get_bandwidth(self.channel)

        @bandwidth.setter
        def bandwidth(self, value):
            return self.dev.set_bandwidth(self.channel, value)

        @property
        def bandwidth_range(self):
            """Range of bandwidths supported by this channel"""
            return self.dev.get_bandwidth_range(self.channel)

        @property
        def frequency(self):
            """Frequency in Hz"""
            return self.dev.get_frequency(self.channel)

        @frequency.setter
        def frequency(self, value):
            return self.dev.set_frequency(self.channel, value)

        @property
        def frequency_range(self):
            """Range of frequencies supported by this channel"""
            return self.dev.get_frequency_range(self.channel)

        @property
        def rf_port(self):
            """Active RF port"""
            return self.dev.get_rf_port(self.channel)

        @rf_port.setter
        def rf_port(self, value):
            return self.dev.set_rf_port(self.channel, value)

        @property
        def rf_ports(self):
            """List of available RF ports"""
            return self.dev.get_rf_ports(self.channel)

        @property
        def bias_tee(self):
            """Bias-T control"""
            return self.dev.get_bias_tee(self.channel)

        @bias_tee.setter
        def bias_tee(self, value):
            return self.dev.set_bias_tee(self.channel, value)

        def set_enable(self, value):
            return self.dev.enable_module(self.channel, value)

        enable = property(fset=set_enable, doc="Enable/disable RF chain")

    def Channel(self, ch):
        """Returns an object for a given subchannel.

        Usage:
        >>> d = bladerf.BladeRF()
        >>> rx = d.Channel(bladerf.CHANNEL_RX(0))

        See help(bladerf.BladeRF._Channel) or help(rx) for more information.
        """
        return self._Channel(self, ch)
