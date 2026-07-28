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

import os
import shutil
import sys
import tempfile
import time
import unittest
from pathlib import Path


PYTHON_BINDINGS = Path(os.environ.get(
    "BLADERF_PYTHON_BINDINGS",
    Path(__file__).resolve().parents[1],
))
sys.path.insert(0, str(PYTHON_BINDINGS))

from bladerf import _bladerf  # noqa: E402


MASTER_SERIAL = os.environ.get("BLADERF_TEST_MASTER_SERIAL")
SLAVE_SERIAL = os.environ.get("BLADERF_TEST_SLAVE_SERIAL")
TRIGGER_CROSSOVER = os.environ.get(
    "BLADERF_TEST_TRIGGER_CROSSOVER") == "1"
GAIN_CALIBRATION_CSV = (
    Path(__file__).resolve().parents[3]
    / "libbladeRF_test/test_gain_calibration/example_measurements/rx_sweep.csv"
)


class HardwareTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if MASTER_SERIAL is None:
            raise unittest.SkipTest("BLADERF_TEST_MASTER_SERIAL is not set")

    def open_master(self):
        return _bladerf.BladeRF("*:serial={}".format(MASTER_SERIAL))

    def open_slave(self):
        if SLAVE_SERIAL is None:
            self.skipTest("BLADERF_TEST_SLAVE_SERIAL is not set")
        return _bladerf.BladeRF("*:serial={}".format(SLAVE_SERIAL))

    def test_cffi_symbols_are_available_with_device_open(self):
        symbols = (
            "bladerf_print_quick_tune",
            "bladerf_format_to_string",
            "bladerf_image_print_metadata",
            "bladerf_image_type_to_string",
            "bladerf_load_gain_calibration",
            "bladerf_print_gain_calibration",
            "bladerf_enable_gain_calibration",
            "bladerf_get_gain_calibration",
            "bladerf_get_gain_target",
        )
        device = self.open_master()
        try:
            self.assertEqual(device.serial, MASTER_SERIAL)
            for symbol in symbols:
                with self.subTest(symbol=symbol):
                    self.assertTrue(hasattr(_bladerf.libbladeRF, symbol))
        finally:
            device.close()

    def test_gain_calibration_round_trip(self):
        channel = _bladerf.CHANNEL_RX(0)
        with tempfile.TemporaryDirectory() as temp_dir:
            calibration_path = Path(temp_dir) / "rx_sweep.csv"
            shutil.copyfile(GAIN_CALIBRATION_CSV, calibration_path)
            device = self.open_slave()
            frequency = device.get_frequency(channel)
            gain_mode = device.get_gain_mode(channel)
            gain = device.get_gain(channel)
            try:
                self.assertEqual(device.serial, SLAVE_SERIAL)
                self.assertTrue(hasattr(device, "load_gain_calibration"))
                device.load_gain_calibration(channel, calibration_path)
                calibration = device.get_gain_calibration(channel)
                target = device.get_gain_target(channel)
                gain_range = device.get_gain_range(channel)
                self.assertEqual(calibration.channel, channel)
                self.assertGreater(len(calibration.entries), 0)
                self.assertEqual(
                    calibration.state,
                    _bladerf.GainCalibrationState.Loaded,
                )
                self.assertGreaterEqual(target, gain_range.min)
                self.assertLessEqual(target, gain_range.max)
            finally:
                if hasattr(device, "enable_gain_calibration"):
                    device.enable_gain_calibration(channel, False)
                device.set_frequency(channel, frequency)
                device.set_gain(channel, gain)
                device.set_gain_mode(channel, gain_mode)
                device.close()

    def test_stream_helpers_round_trip(self):
        channel = _bladerf.CHANNEL_RX(0)
        device = self.open_master()
        sample_rate = device.get_sample_rate(channel)
        stream_timeout = None
        try:
            requested = _bladerf.RationalRate(2000000, 1, 3)
            actual = device.set_rational_sample_rate(channel, requested)
            current = device.get_rational_sample_rate(channel)
            self.assertEqual(actual, current)

            samples = bytearray(range(32))
            original = bytes(samples)
            device.interleave_stream_buffer(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                8,
                samples,
            )
            self.assertNotEqual(samples, original)
            device.deinterleave_stream_buffer(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                8,
                samples,
            )
            self.assertEqual(samples, original)

            stream_timeout = device.get_stream_timeout(
                _bladerf.Direction.RX)
            device.set_stream_timeout(_bladerf.Direction.RX, 987)
            self.assertEqual(
                device.get_stream_timeout(_bladerf.Direction.RX),
                987,
            )
        finally:
            if stream_timeout is not None:
                device.set_stream_timeout(
                    _bladerf.Direction.RX, stream_timeout)
            device.set_sample_rate(channel, sample_rate)
            device.close()

    def test_device_modes_round_trip(self):
        device = self.open_master()
        tamer_mode = None
        tuning_mode = None
        try:
            try:
                tamer_mode = device.vctcxo_tamer_mode
            except _bladerf.UnsupportedError:
                pass
            tuning_mode = device.tuning_mode
            trim = device.vctcxo
            if tamer_mode is not None:
                device.vctcxo_tamer_mode = tamer_mode
            device.tuning_mode = tuning_mode
            if tamer_mode is not None:
                self.assertEqual(device.vctcxo_tamer_mode, tamer_mode)
            self.assertEqual(device.tuning_mode, tuning_mode)
            self.assertGreaterEqual(trim, 0)
            self.assertLessEqual(trim, 0xffff)
        finally:
            if tamer_mode is not None:
                device.vctcxo_tamer_mode = tamer_mode
            if tuning_mode is not None:
                device.tuning_mode = tuning_mode
            device.close()

    def test_metadata_receive_and_timestamp(self):
        channel = _bladerf.CHANNEL_RX(0)
        device = self.open_master()
        enabled = False
        try:
            self.assertTrue(hasattr(device, "get_timestamp"))
            device.sync_config(
                _bladerf.ChannelLayout.RX_X1,
                _bladerf.Format.SC16_Q11_META,
                16,
                2048,
                8,
                3500,
            )
            device.enable_module(channel, True)
            enabled = True
            timestamp_before = device.get_timestamp(_bladerf.Direction.RX)
            metadata = device.sync_rx(
                bytearray(8192),
                2048,
                3500,
                _bladerf.Metadata(
                    0,
                    _bladerf.MetadataFlags.RX_NOW,
                    _bladerf.MetadataStatus.NONE,
                    0,
                ),
            )
            timestamp_after = device.get_timestamp(_bladerf.Direction.RX)
            self.assertGreater(metadata.timestamp, 0)
            self.assertEqual(metadata.actual_count, 2048)
            self.assertGreater(timestamp_after, timestamp_before)
        finally:
            if enabled:
                device.enable_module(channel, False)
            device.close()

    def test_dual_channel_receive_buffer(self):
        channels = (
            _bladerf.CHANNEL_RX(0),
            _bladerf.CHANNEL_RX(1),
        )
        device = self.open_master()
        enabled = []
        try:
            device.sync_config(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                16,
                2048,
                8,
                3500,
            )
            for channel in channels:
                device.enable_module(channel, True)
                enabled.append(channel)
            device.sync_rx(bytearray(8192), 2048, 3500)
            with self.assertRaisesRegex(ValueError, "8192 bytes required"):
                device.sync_rx(bytearray(8191), 2048, 3500)
        finally:
            for channel in reversed(enabled):
                device.enable_module(channel, False)
            device.close()

    def test_dual_channel_counter_integrity(self):
        channels = (
            _bladerf.CHANNEL_RX(0),
            _bladerf.CHANNEL_RX(1),
        )
        device = self.open_master()
        rx_mux = device.rx_mux
        enabled = []
        num_samples = 65536
        received = bytearray(num_samples * 4)

        try:
            device.rx_mux = _bladerf.RXMux.Counter_12bit
            device.sync_config(
                _bladerf.ChannelLayout.RX_X2,
                _bladerf.Format.SC16_Q11,
                16,
                8192,
                8,
                3500,
            )
            for channel in channels:
                device.enable_module(channel, True)
                enabled.append(channel)

            device.sync_rx(received, num_samples, 3500)
            samples = memoryview(received).cast("h")
            for offset in range(4):
                values = [sample & 0x0fff for sample in samples[offset::4]]
                pairs = list(zip(values, values[1:]))
                step = 1 if offset % 2 == 0 else 0x0fff
                wrap = ((0x07ff, 0x0801) if step == 1
                        else (0x0801, 0x07ff))
                invalid = [
                    pair for pair in pairs
                    if ((pair[1] - pair[0]) & 0x0fff) != step
                    and pair != wrap
                ]
                with self.subTest(component=offset):
                    self.assertEqual(invalid, [])
                    self.assertIn(wrap, pairs)
        finally:
            for channel in enabled:
                device.enable_module(channel, False)
            device.rx_mux = rx_mux
            device.close()

    def test_packed_receive_buffer(self):
        device = self.open_master()
        channel = _bladerf.CHANNEL_RX(0)
        enabled = False
        try:
            self.assertTrue(hasattr(_bladerf.Format, "SC16_Q11_PACKED"))
            self.assertEqual(
                _bladerf.Format.SC16_Q11_PACKED.value,
                _bladerf.libbladeRF.BLADERF_FORMAT_SC16_Q11_PACKED,
            )
            device.sync_config(
                _bladerf.ChannelLayout.RX_X1,
                _bladerf.Format.SC16_Q11_PACKED,
                16,
                8192,
                8,
                3500,
            )
            device.enable_module(channel, True)
            enabled = True
            samples = bytearray(32768)
            device.sync_rx(samples, 8192, 3500)
            self.assertTrue(all(
                -2048 <= sample <= 2047
                for sample in memoryview(samples).cast("h")
            ))
            with self.assertRaisesRegex(ValueError, "32768 bytes required"):
                device.sync_rx(bytearray(32767), 8192, 3500)
        finally:
            if enabled:
                device.enable_module(channel, False)
            device.close()

    def test_feature_control_round_trip(self):
        device = self.open_master()
        original = None
        try:
            self.assertTrue(hasattr(device, "enable_feature"))
            original = device.feature
            enable = original != _bladerf.Feature.OVERSAMPLE
            device.enable_feature(_bladerf.Feature.OVERSAMPLE, enable)
            expected = (_bladerf.Feature.OVERSAMPLE
                        if enable else _bladerf.Feature.DEFAULT)
            self.assertEqual(device.feature, expected)
        finally:
            if original is not None:
                device.enable_feature(
                    _bladerf.Feature.OVERSAMPLE,
                    original == _bladerf.Feature.OVERSAMPLE,
                )
            device.close()

    def test_trigger_control(self):
        device = self.open_master()
        trigger = None
        try:
            trigger = device.trigger_init(
                _bladerf.CHANNEL_RX(0),
                _bladerf.TriggerSignal.MiniExp1,
            )
            trigger.role = _bladerf.TriggerRole.Master
            device.trigger_arm(trigger, True)
            self.assertTrue(device.trigger_state(trigger).is_armed)
            device.trigger_fire(trigger)
            state = device.trigger_state(trigger)
            self.assertTrue(state.has_fired)
            self.assertTrue(state.fire_requested)
        finally:
            if trigger is not None:
                trigger.role = _bladerf.TriggerRole.Disabled
                device.trigger_arm(trigger, False)
            device.close()

    @unittest.skipUnless(
        TRIGGER_CROSSOVER,
        "BLADERF_TEST_TRIGGER_CROSSOVER is not set to 1",
    )
    def test_trigger_crossover(self):
        master = self.open_master()
        slave = self.open_slave()
        master_clock_output = master.clock_output
        slave_clock_select = slave.clock_select
        master_trigger = None
        slave_trigger = None
        try:
            master.clock_output = True
            time.sleep(0.1)
            slave.clock_select = _bladerf.ClockSelect.External
            time.sleep(0.1)
            self.assertTrue(hasattr(master, "trigger_init"))

            master_trigger = master.trigger_init(
                _bladerf.CHANNEL_RX(0),
                _bladerf.TriggerSignal.MiniExp1,
            )
            slave_trigger = slave.trigger_init(
                _bladerf.CHANNEL_RX(0),
                _bladerf.TriggerSignal.MiniExp1,
            )
            master_trigger.role = _bladerf.TriggerRole.Master
            slave_trigger.role = _bladerf.TriggerRole.Slave
            slave.trigger_arm(slave_trigger, True)
            master.trigger_arm(master_trigger, True)
            self.assertTrue(
                master.trigger_state(master_trigger).is_armed)
            self.assertTrue(
                slave.trigger_state(slave_trigger).is_armed)

            master.trigger_fire(master_trigger)
            time.sleep(0.1)
            master_state = master.trigger_state(master_trigger)
            slave_state = slave.trigger_state(slave_trigger)
            self.assertTrue(master_state.has_fired)
            self.assertTrue(master_state.fire_requested)
            self.assertTrue(slave_state.has_fired)
        finally:
            if master_trigger is not None:
                master_trigger.role = _bladerf.TriggerRole.Disabled
                master.trigger_arm(master_trigger, False)
            if slave_trigger is not None:
                slave_trigger.role = _bladerf.TriggerRole.Disabled
                slave.trigger_arm(slave_trigger, False)
            slave.clock_select = slave_clock_select
            master.clock_output = master_clock_output
            slave.close()
            master.close()

if __name__ == "__main__":
    unittest.main()
