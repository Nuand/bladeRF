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
