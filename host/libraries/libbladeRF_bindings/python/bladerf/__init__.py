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


__doc__ = """
CFFI-based Python 3 binding to libbladeRF

Implements a Pythonic binding to libbladeRF, the library for interfacing
with the Nuand bladeRF product line.

See help(bladerf.BladeRF) for more information.

Basic usage:
>>> import bladerf
>>> b = bladerf.BladeRF()
"""

from bladerf import _bladerf
from bladerf import _tool

BladeRF = _bladerf.BladeRF

BladeRFError = _bladerf.BladeRFError
ChannelLayout = _bladerf.ChannelLayout
ClockSelect = _bladerf.ClockSelect
Direction = _bladerf.Direction
Feature = _bladerf.Feature
Format = _bladerf.Format
GainMode = _bladerf.GainMode
Loopback = _bladerf.Loopback
Metadata = _bladerf.Metadata
MetadataFlags = _bladerf.MetadataFlags
MetadataStatus = _bladerf.MetadataStatus
RationalRate = _bladerf.RationalRate
Trigger = _bladerf.Trigger
TriggerRole = _bladerf.TriggerRole
TriggerSignal = _bladerf.TriggerSignal
TuningMode = _bladerf.TuningMode
VCTCXOTamerMode = _bladerf.VCTCXOTamerMode

allocate_buffer = _bladerf.allocate_buffer
get_bootloader_list = _bladerf.get_bootloader_list
get_device_list = _bladerf.get_device_list
load_fw_from_bootloader = _bladerf.load_fw_from_bootloader
sample_buffer_size = _bladerf.sample_buffer_size
set_verbosity = _bladerf.set_verbosity
version = _bladerf.version

RX = _bladerf.RX
TX = _bladerf.TX
CHANNEL_RX = _bladerf.CHANNEL_RX
CHANNEL_TX = _bladerf.CHANNEL_TX

main = _tool.main

__all__ = [
    'BladeRF', 'BladeRFError', 'ChannelLayout', 'ClockSelect', 'Direction',
    'Feature', 'Format', 'GainMode', 'Loopback', 'Metadata', 'MetadataFlags',
    'MetadataStatus', 'RationalRate', 'Trigger', 'TriggerRole', 'TriggerSignal',
    'TuningMode', 'VCTCXOTamerMode', 'allocate_buffer', 'get_bootloader_list',
    'get_device_list', 'load_fw_from_bootloader', 'sample_buffer_size',
    'set_verbosity', 'version', 'RX', 'TX', 'CHANNEL_RX', 'CHANNEL_TX', 'main',
]
