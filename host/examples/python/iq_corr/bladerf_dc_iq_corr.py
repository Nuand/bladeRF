#!/usr/bin/python2
#!/usr/bin/env python2
#
# This script puts the bladeRF into an RF loopback mode, transmitting
# a vector at an offset from the RX frequency. The resulting image may be used
# to manually dial in RX-side IQ imbalance correction. Sliders to minimize the RX
# DC offset are also included.
#
# License: GPLv3
# 
# The original version of this script was downloaded from 
# https://gist.github.com/jynik/9503066 on 2022-02-17.
#
# This script was derived from the gr-osmosdr's osmocom_fft script.
#    http://sdr.osmocom.org/trac/wiki/GrOsmoSDR
#
# The copyright information from osmocom_fft is provided below.
# -----------------------------------------------------------------------------
# Copyright 2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

DEFAULT_RX_FREQ = 300e6
DEFAULT_SAMPLE_RATE = 24e5
DEFAULT_TX_FREQ_OFFSET = (DEFAULT_SAMPLE_RATE / 4)
DEFAULT_FFT_SIZE = 2048


KEY_RX_FREQ = 'rx_freq'
KEY_TX_FREQ_OFF = 'tx_freq_off'
KEY_SAMPLE_RATE = 'sample_rate'
KEY_DC_OFF_I = 'dc_offset_real'
KEY_DC_OFF_Q = 'dc_offset_imag'
KEY_IQ_BAL_I = 'iq_balance_mag'
KEY_IQ_BAL_Q = 'iq_balance_pha'

import osmosdr
from gnuradio import gr, gru, eng_notation, analog
from gnuradio.gr.pubsub import pubsub
from gnuradio.eng_option import eng_option
from optparse import OptionParser

try:
    from gnuradio.wxgui import stdgui2, form, slider, forms, fftsink2, scopesink2
    import wx

except ImportError:
    sys.stderr.write('Error importing GNU Radio\'s wxgui. Please make sure gr-wxgui is installed.\n')
    sys.exit(1)

class top_block(stdgui2.std_top_block, pubsub):
    def __init__(self, frame, panel, vbox, argv):
        stdgui2.std_top_block.__init__(self, frame, panel, vbox, argv)
        pubsub.__init__(self)

        self.frame = frame
        self.panel = panel

        parser = OptionParser(option_class=eng_option)

        parser.add_option('-a', '--args', type='string', default='bladerf=0',
                          help='Device args, [default=%default]')

        parser.add_option('-s', '--sample-rate', type='eng_float', default=DEFAULT_SAMPLE_RATE,
                          help='Sample rate [default=%default]')

        parser.add_option('-f', '--rx-freq', type='eng_float', default=DEFAULT_RX_FREQ,
                          help='RX frequency [default=%default]')

        parser.add_option('-o', '--tx-freq-off', type='eng_float', default=DEFAULT_TX_FREQ_OFFSET,
                          help='TX frequency offset [default=%default]')

        parser.add_option("", "--avg-alpha", type="eng_float", default=1e-1,
                          help="Set fftsink averaging factor, default=[%default]")

        parser.add_option("", "--averaging", action="store_true", default=False,
                          help="Enable fftsink averaging, default=[%default]")

        parser.add_option("", "--ref-scale", type="eng_float", default=1.0,
                          help="Set dBFS=0dB input value, default=[%default]")

        parser.add_option("", "--fft-size", type="int", default=DEFAULT_FFT_SIZE,
                          help="Set number of FFT bins [default=%default]")

        parser.add_option("", "--fft-rate", type="int", default=30,
                          help="Set FFT update rate, [default=%default]")

        parser.add_option("-v", "--verbose", action="store_true", default=False,
                          help="Use verbose console output [default=%default]")

        (options, args) = parser.parse_args()
        if len(args) != 0:
            parser.print_help()
            sys.exit(1)
        self.options = options

        if options.rx_freq >= 1.5e6:
            options.args += ',loopback=rf_lna1'
        else:
            options.args += ',loopback=rf_lna2'

        self._verbose = options.verbose

        self.src = osmosdr.source(options.args)
        self.sink = osmosdr.sink(options.args)

        self.src.set_sample_rate(options.sample_rate)
        self.sink.set_sample_rate(options.sample_rate)

        self.src.set_bandwidth(0.90 * options.sample_rate)
        self.sink.set_bandwidth(0.90 * options.sample_rate)

        self[KEY_RX_FREQ] = options.rx_freq
        self[KEY_TX_FREQ_OFF] = options.tx_freq_off
        self[KEY_DC_OFF_I] = 0
        self[KEY_DC_OFF_Q] = 0
        self[KEY_IQ_BAL_I] = 0
        self[KEY_IQ_BAL_Q] = 0

        self.subscribe(KEY_RX_FREQ, self.set_freq)
        self.subscribe(KEY_DC_OFF_I, self.set_dc_offset)
        self.subscribe(KEY_DC_OFF_Q, self.set_dc_offset)
        self.subscribe(KEY_IQ_BAL_I, self.set_iq_balance)
        self.subscribe(KEY_IQ_BAL_Q, self.set_iq_balance)

        self.fft = fftsink2.fft_sink_c(panel,
                                         fft_size=options.fft_size,
                                         sample_rate=options.sample_rate,
                                         ref_scale=options.ref_scale,
                                         ref_level=0,
                                         y_divs=10,
                                         average=options.averaging,
                                         avg_alpha=options.avg_alpha,
                                         fft_rate=options.fft_rate)

        self.fft.set_callback(self.wxsink_callback)
        self.connect(self.src, self.fft)

        self.scope = scopesink2.scope_sink_c(panel,
                                             sample_rate = options.sample_rate,
                                             v_scale = 0,
                                             v_offest = 0,
                                             t_scale = 0,
                                             ac_couple = False,
                                             xy_mode = True,
                                             num_inputs = 1)
        self.connect(self.src, self.scope)


        self.constant_source = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, 1)
        self.connect(self.constant_source, self.sink)

        self.set_freq(options.rx_freq)

        self.frame.SetMinSize((800, 420))
        self._build_gui(vbox)

    def set_dc_offset(self, value):
        correction = complex(self[KEY_DC_OFF_I], self[KEY_DC_OFF_Q])
        try:
            self.src.set_dc_offset(correction)
            if self._verbose:
                print "Set DC offset to", correction
        except RuntimeError as ex:
            print ex

    def set_iq_balance(self, value):
        correction = complex(self[KEY_IQ_BAL_I], self[KEY_IQ_BAL_Q])
        try:
            self.src.set_iq_balance(correction)
            if self._verbose:
                print "Set IQ balance to", correction
        except RuntimeError as ex:
            print ex

    def wxsink_callback(self, x, y):
        self.set_freq_from_callback(x)

    def set_freq_from_callback(self, freq):
        freq = self.src.set_center_freq(freq)
        self[KEY_RX_FREQ] = freq;

    def set_freq(self, freq):
        if freq is None:
            f = self[FREQ_RANGE_KEY]
            freq = float(f.start()+f.stop())/2.0
            if self._verbose:
                print "Using auto-calculated mid-point frequency"
            self[KEY_RX_FREQ] = freq
            return

        self.sink.set_center_freq(freq + self[KEY_TX_FREQ_OFF])
        freq = self.src.set_center_freq(freq)

        if hasattr(self.fft, 'set_baseband_freq'):
            self.fft.set_baseband_freq(freq)

        if freq is not None:
            if self._verbose:
                print "Set center frequency to", freq
        elif self._verbose:
            print "Failed to set freq."
        return freq

    def _build_gui(self, vbox):
        vbox.Add(self.fft.win, 1, wx.EXPAND)
        vbox.Add(self.scope.win, 1, wx.EXPAND)
        vbox.AddSpacer(3)

        # add control area at the bottom
        self.myform = myform = form.form()

        ##################################################
        # DC Offset controls
        ##################################################

        dc_offset_vbox = forms.static_box_sizer(parent=self.panel,
                label="DC Offset Correction",
                orient=wx.VERTICAL,
                bold=True)
        dc_offset_vbox.AddSpacer(3)
        # First row of sample rate controls
        dc_offset_hbox = wx.BoxSizer(wx.HORIZONTAL)
        dc_offset_vbox.Add(dc_offset_hbox, 0, wx.EXPAND)
        dc_offset_vbox.AddSpacer(3)

        # Add frequency controls to top window sizer
        vbox.Add(dc_offset_vbox, 0, wx.EXPAND)
        vbox.AddSpacer(3)

        dc_offset_hbox.AddSpacer(3)
        self.dc_offset_real_text = forms.text_box(
                parent=self.panel, sizer=dc_offset_hbox,
                label='Real',
                proportion=1,
                converter=forms.float_converter(),
                ps=self,
                key='dc_offset_real',
                )
        dc_offset_hbox.AddSpacer(3)

        self.dc_offset_real_slider = forms.slider(
                parent=self.panel, sizer=dc_offset_hbox,
                proportion=3,
                minimum=-1,
                maximum=+1,
                step_size=0.0005,
                ps=self,
                key='dc_offset_real',
                )
        dc_offset_hbox.AddSpacer(3)

        dc_offset_hbox.AddSpacer(3)
        self.dc_offset_imag_text = forms.text_box(
                parent=self.panel, sizer=dc_offset_hbox,
                label='Imag',
                proportion=1,
                converter=forms.float_converter(),
                ps=self,
                key='dc_offset_imag',
                )
        dc_offset_hbox.AddSpacer(3)

        self.dc_offset_imag_slider = forms.slider(
                parent=self.panel, sizer=dc_offset_hbox,
                proportion=3,
                minimum=-1,
                maximum=+1,
                step_size=0.0005,
                ps=self,
                key='dc_offset_imag',
                )
        dc_offset_hbox.AddSpacer(3)

        ##################################################
        # IQ Imbalance controls
        ##################################################
        iq_balance_vbox = forms.static_box_sizer(parent=self.panel,
                                         label="IQ Imbalance Correction",
                                         orient=wx.VERTICAL,
                                         bold=True)
        iq_balance_vbox.AddSpacer(3)
        # First row of sample rate controls
        iq_balance_hbox = wx.BoxSizer(wx.HORIZONTAL)
        iq_balance_vbox.Add(iq_balance_hbox, 0, wx.EXPAND)
        iq_balance_vbox.AddSpacer(3)

        # Add frequency controls to top window sizer
        vbox.Add(iq_balance_vbox, 0, wx.EXPAND)
        vbox.AddSpacer(3)

        iq_balance_hbox.AddSpacer(3)
        self.iq_balance_mag_text = forms.text_box(
            parent=self.panel, sizer=iq_balance_hbox,
            label='Mag',
            proportion=1,
            converter=forms.float_converter(),
            ps=self,
            key=KEY_IQ_BAL_I,
        )
        iq_balance_hbox.AddSpacer(3)

        self.iq_balance_mag_slider = forms.slider(
            parent=self.panel, sizer=iq_balance_hbox,
            proportion=3,
            minimum=-1,
            maximum=+1,
            step_size=0.00025,
            ps=self,
            key=KEY_IQ_BAL_I,
        )
        iq_balance_hbox.AddSpacer(3)

        iq_balance_hbox.AddSpacer(3)
        self.iq_balance_pha_text = forms.text_box(
            parent=self.panel, sizer=iq_balance_hbox,
            label='Phase',
            proportion=1,
            converter=forms.float_converter(),
            ps=self,
            key=KEY_IQ_BAL_Q,
        )
        iq_balance_hbox.AddSpacer(3)

        self.iq_balance_pha_slider = forms.slider(
            parent=self.panel, sizer=iq_balance_hbox,
            proportion=3,
            minimum=-1,
            maximum=+1,
            step_size=0.00025,
            ps=self,
            key=KEY_IQ_BAL_Q,
        )
        iq_balance_hbox.AddSpacer(3)

def main():
    app = stdgui2.stdapp(top_block, 'bladeRF: manual DC Offset and IQ Imbalance correction', nstatus=1)
    app.MainLoop()

if __name__ == '__main__':
    main ()