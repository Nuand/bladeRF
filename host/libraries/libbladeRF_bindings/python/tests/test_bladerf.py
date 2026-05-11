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


if __name__ == "__main__":
    unittest.main()
