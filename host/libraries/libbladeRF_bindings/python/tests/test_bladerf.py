# Copyright (c) 2026 Nuand LLC
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

import sys
import unittest
from pathlib import Path
from unittest import mock


PYTHON_BINDINGS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PYTHON_BINDINGS))

from bladerf import _bladerf  # noqa: E402


def _device():
    device = _bladerf.BladeRF.__new__(_bladerf.BladeRF)
    device.dev = _bladerf.ffi.new("struct bladerf *[1]")
    return device


class FormatTest(unittest.TestCase):
    def test_format_values_match_libbladerf_abi(self):
        self.assertEqual(
            [fmt.value for fmt in _bladerf.Format],
            [0, 1, 2, 3, 4, 5],
        )
        self.assertEqual(_bladerf.Format.SC16_Q11_PACKED.value, 1)


class FeatureTest(unittest.TestCase):
    def test_feature_round_trip(self):
        calls = []

        class Lib:
            @staticmethod
            def bladerf_get_feature(dev, feature):
                feature[0] = _bladerf.Feature.OVERSAMPLE.value
                return 0

            @staticmethod
            def bladerf_enable_feature(dev, feature, enable):
                calls.append((feature, enable))
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            self.assertEqual(device.feature, _bladerf.Feature.OVERSAMPLE)
            device.enable_feature(_bladerf.Feature.OVERSAMPLE)

        self.assertEqual(calls, [(_bladerf.Feature.OVERSAMPLE.value, True)])


class GainCalibrationTest(unittest.TestCase):
    def test_gain_calibration_round_trip(self):
        calls = []
        entries = _bladerf.ffi.new("struct bladerf_gain_cal_entry[]", 2)
        entries[0].freq = 100000000
        entries[0].gain_corr = 1.25
        entries[1].freq = 200000000
        entries[1].gain_corr = -0.5
        file_path = _bladerf.ffi.new("char[]", b"rx1.tbl")
        table = _bladerf.ffi.new("struct bladerf_gain_cal_tbl *")
        table.version.major = 1
        table.version.describe = _bladerf.ffi.NULL
        table.ch = _bladerf.CHANNEL_RX(0)
        table.enabled = True
        table.n_entries = 2
        table.start_freq = entries[0].freq
        table.stop_freq = entries[1].freq
        table.entries = entries
        table.gain_target = 17
        table.file_path_len = len(b"rx1.tbl")
        table.file_path = file_path
        table.state = 1

        class Lib:
            BLADERF_GAIN_CAL_UNINITIALIZED = 0
            BLADERF_GAIN_CAL_LOADED = 1
            BLADERF_GAIN_CAL_UNLOADED = 2

            @staticmethod
            def bladerf_load_gain_calibration(dev, ch, path):
                calls.append(("load", ch, path))
                return 0

            @staticmethod
            def bladerf_print_gain_calibration(dev, ch, with_entries):
                calls.append(("print", ch, bool(with_entries)))
                return 0

            @staticmethod
            def bladerf_enable_gain_calibration(dev, ch, enable):
                calls.append(("enable", ch, bool(enable)))
                return 0

            @staticmethod
            def bladerf_get_gain_calibration(dev, ch, result):
                result[0] = table
                return 0

            @staticmethod
            def bladerf_get_gain_target(dev, ch, target):
                target[0] = 17
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            channel = _bladerf.CHANNEL_RX(0)
            device.load_gain_calibration(channel, "rx1.tbl")
            device.print_gain_calibration(channel, True)
            device.enable_gain_calibration(channel, True)
            calibration = device.get_gain_calibration(channel)
            target = device.get_gain_target(channel)

        self.assertEqual(
            calibration.entries,
            (
                _bladerf.GainCalibrationEntry(100000000, 1.25),
                _bladerf.GainCalibrationEntry(200000000, -0.5),
            ),
        )
        self.assertEqual(calibration.version.major, 1)
        self.assertEqual(calibration.version.describe, "")
        self.assertEqual(calibration.channel, channel)
        self.assertTrue(calibration.enabled)
        self.assertEqual(calibration.file_path, "rx1.tbl")
        self.assertEqual(
            calibration.state,
            _bladerf.GainCalibrationState.Loaded,
        )
        self.assertEqual(calibration.gain_target, target)
        self.assertEqual(
            calls,
            [
                ("load", channel, b"rx1.tbl"),
                ("print", channel, True),
                ("enable", channel, True),
            ],
        )


class StreamHelperTest(unittest.TestCase):
    def test_rational_rates_and_stream_helpers(self):
        calls = []

        class Lib:
            @staticmethod
            def bladerf_set_rational_sample_rate(dev, ch, rate, actual):
                actual[0] = rate[0]
                calls.append(("set_rate", ch))
                return 0

            @staticmethod
            def bladerf_get_rational_sample_rate(dev, ch, rate):
                rate.integer = 2000000
                rate.num = 1
                rate.den = 2
                return 0

            @staticmethod
            def bladerf_interleave_stream_buffer(layout, fmt, size, samples):
                _bladerf.ffi.cast("uint8_t *", samples)[0] = 0x5a
                calls.append(("interleave", layout, fmt, size))
                return 0

            @staticmethod
            def bladerf_deinterleave_stream_buffer(layout, fmt, size, samples):
                calls.append(("deinterleave", layout, fmt, size))
                return 0

            @staticmethod
            def bladerf_set_stream_timeout(dev, direction, timeout):
                calls.append(("set_timeout", direction, timeout))
                return 0

            @staticmethod
            def bladerf_get_stream_timeout(dev, direction, timeout):
                timeout[0] = 2500
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            requested = _bladerf.RationalRate(1000000, 1, 3)
            actual = device.set_rational_sample_rate(
                _bladerf.CHANNEL_RX(0),
                requested,
            )
            current = device.get_rational_sample_rate(
                _bladerf.CHANNEL_RX(0),
            )
            samples = bytearray(16)
            device.interleave_stream_buffer(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                2,
                samples,
            )
            device.deinterleave_stream_buffer(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                2,
                samples,
            )
            device.set_stream_timeout(_bladerf.Direction.RX, 2500)
            timeout = device.get_stream_timeout(_bladerf.Direction.RX)

        self.assertEqual(actual, requested)
        self.assertEqual(current, _bladerf.RationalRate(2000000, 1, 2))
        self.assertEqual(samples[0], 0x5a)
        self.assertEqual(timeout, 2500)
        self.assertEqual(
            calls,
            [
                ("set_rate", _bladerf.CHANNEL_RX(0)),
                (
                    "interleave",
                    _bladerf.ChannelLayout.RX_X2.value,
                    _bladerf.Format.SC16_Q11.value,
                    2,
                ),
                (
                    "deinterleave",
                    _bladerf.ChannelLayout.RX_X2.value,
                    _bladerf.Format.SC16_Q11.value,
                    2,
                ),
                ("set_timeout", _bladerf.Direction.RX.value, 2500),
            ],
        )


class MetadataTest(unittest.TestCase):
    def test_metadata_and_timestamp_round_trip(self):
        calls = []

        class Lib:
            @staticmethod
            def bladerf_get_timestamp(dev, direction, timestamp):
                calls.append(("timestamp", direction))
                timestamp[0] = 123456
                return 0

            @staticmethod
            def bladerf_sync_tx(dev, samples, count, metadata, timeout):
                calls.append(("tx", count, metadata.timestamp, metadata.flags))
                return 0

            @staticmethod
            def bladerf_sync_rx(dev, samples, count, metadata, timeout):
                metadata.timestamp = 654321
                metadata.status = 1 << 16
                metadata.actual_count = count
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            timestamp = device.get_timestamp(_bladerf.Direction.RX)
            tx_metadata = _bladerf.Metadata(
                123456,
                _bladerf.MetadataFlags.TX_BURST_START
                | _bladerf.MetadataFlags.TX_NOW,
                _bladerf.MetadataStatus.NONE,
                0,
            )
            device.sync_tx(bytearray(16), 4, 1000, tx_metadata)
            rx_metadata = device.sync_rx(
                bytearray(16),
                4,
                1000,
                _bladerf.Metadata(
                    0,
                    _bladerf.MetadataFlags.RX_NOW,
                    _bladerf.MetadataStatus.NONE,
                    0,
                ),
            )

        self.assertEqual(timestamp, 123456)
        self.assertEqual(rx_metadata.timestamp, 654321)
        self.assertEqual(rx_metadata.actual_count, 4)
        self.assertEqual(
            rx_metadata.status,
            _bladerf.MetadataStatus.RX_HW_MINIEXP1,
        )
        self.assertEqual(
            calls,
            [
                ("timestamp", _bladerf.Direction.RX.value),
                (
                    "tx",
                    4,
                    123456,
                    int(
                        _bladerf.MetadataFlags.TX_BURST_START
                        | _bladerf.MetadataFlags.TX_NOW
                    ),
                ),
            ],
        )


class DeviceModeTest(unittest.TestCase):
    def test_vctcxo_tamer_and_tuning_modes(self):
        calls = []

        class Lib:
            @staticmethod
            def bladerf_set_vctcxo_tamer_mode(dev, mode):
                calls.append(("tamer", mode))
                return 0

            @staticmethod
            def bladerf_get_vctcxo_tamer_mode(dev, mode):
                mode[0] = _bladerf.VCTCXOTamerMode.PPS_1.value
                return 0

            @staticmethod
            def bladerf_get_vctcxo_trim(dev, trim):
                trim[0] = 0x7ff0
                return 0

            @staticmethod
            def bladerf_set_tuning_mode(dev, mode):
                calls.append(("tuning", mode))
                return 0

            @staticmethod
            def bladerf_get_tuning_mode(dev, mode):
                mode[0] = _bladerf.TuningMode.FPGA.value
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            device.vctcxo_tamer_mode = _bladerf.VCTCXOTamerMode.PPS_1
            tamer_mode = device.vctcxo_tamer_mode
            trim = device.vctcxo
            device.tuning_mode = _bladerf.TuningMode.FPGA
            tuning_mode = device.tuning_mode

        self.assertEqual(tamer_mode, _bladerf.VCTCXOTamerMode.PPS_1)
        self.assertEqual(trim, 0x7ff0)
        self.assertEqual(tuning_mode, _bladerf.TuningMode.FPGA)
        self.assertEqual(
            calls,
            [
                ("tamer", _bladerf.VCTCXOTamerMode.PPS_1.value),
                ("tuning", _bladerf.TuningMode.FPGA.value),
            ],
        )


class TriggerTest(unittest.TestCase):
    def test_trigger_control_round_trip(self):
        calls = []

        class Lib:
            @staticmethod
            def bladerf_trigger_init(dev, ch, signal, trigger):
                trigger.channel = ch
                trigger.role = _bladerf.TriggerRole.Disabled.value
                trigger.signal = signal
                return 0

            @staticmethod
            def bladerf_trigger_arm(dev, trigger, arm, resv1, resv2):
                calls.append(("arm", bool(arm), resv1, resv2))
                return 0

            @staticmethod
            def bladerf_trigger_fire(dev, trigger):
                calls.append(("fire", trigger.channel))
                return 0

            @staticmethod
            def bladerf_trigger_state(dev, trigger, armed, fired, requested,
                                      resv1, resv2):
                armed[0] = True
                fired[0] = False
                requested[0] = True
                return 0

            @staticmethod
            def bladerf_read_trigger(dev, ch, signal, value):
                value[0] = 0xa5
                return 0

            @staticmethod
            def bladerf_write_trigger(dev, ch, signal, value):
                calls.append(("write", ch, signal, value))
                return 0

        with mock.patch.object(_bladerf, "libbladeRF", Lib):
            device = _device()
            trigger = device.trigger_init(
                _bladerf.CHANNEL_RX(0),
                _bladerf.TriggerSignal.MiniExp1,
            )
            trigger.role = _bladerf.TriggerRole.Master
            trigger.options = 7
            device.trigger_arm(trigger, True, 1, 2)
            device.trigger_fire(trigger)
            state = device.trigger_state(trigger)
            value = device.read_trigger(
                trigger.channel,
                _bladerf.TriggerSignal.MiniExp1,
            )
            device.write_trigger(
                trigger.channel,
                _bladerf.TriggerSignal.MiniExp1,
                0x5a,
            )

        self.assertEqual(trigger.role, _bladerf.TriggerRole.Master)
        self.assertEqual(trigger.signal, _bladerf.TriggerSignal.MiniExp1)
        self.assertEqual(trigger.options, 7)
        self.assertEqual(state, _bladerf.TriggerState(True, False, True))
        self.assertEqual(value, 0xa5)
        self.assertEqual(
            calls,
            [
                ("arm", True, 1, 2),
                ("fire", _bladerf.CHANNEL_RX(0)),
                (
                    "write",
                    _bladerf.CHANNEL_RX(0),
                    _bladerf.TriggerSignal.MiniExp1.value,
                    0x5a,
                ),
            ],
        )


if __name__ == "__main__":
    unittest.main()
