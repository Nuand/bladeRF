#!/usr/bin/env python2
##################################################
# GNU Radio Python Flow Graph
# Title: Simple bladeRF RX GUI
# Author: Nuand, LLC <bladeRF@nuand.com>
# Description: A simple RX-only GUI that demonstrates the usage of various RX controls.
# Generated: Sun Jan 17 21:00:48 2016
##################################################

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print "Warning: failed to XInitThreads()"

from PyQt4 import Qt
from PyQt4.QtCore import QObject, pyqtSlot
from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import qtgui
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from gnuradio.qtgui import Range, RangeWidget
from optparse import OptionParser
import osmosdr
import sip
import sys
import time


class bladeRF_rx(gr.top_block, Qt.QWidget):

    def __init__(self, buflen=4096, dc_offset_i=0, dc_offset_q=0, instance=0, num_buffers=16, num_xfers=8, rx_bandwidth=1.5e6, rx_frequency=915e6, rx_lna_gain=6, rx_sample_rate=3e6, rx_vga_gain=20, serial="", verbosity="info"):
        gr.top_block.__init__(self, "Simple bladeRF RX GUI")
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Simple bladeRF RX GUI")
        try:
             self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except:
             pass
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("GNU Radio", "bladeRF_rx")
        self.restoreGeometry(self.settings.value("geometry").toByteArray())

        ##################################################
        # Parameters
        ##################################################
        self.buflen = buflen
        self.dc_offset_i = dc_offset_i
        self.dc_offset_q = dc_offset_q
        self.instance = instance
        self.num_buffers = num_buffers
        self.num_xfers = num_xfers
        self.rx_bandwidth = rx_bandwidth
        self.rx_frequency = rx_frequency
        self.rx_lna_gain = rx_lna_gain
        self.rx_sample_rate = rx_sample_rate
        self.rx_vga_gain = rx_vga_gain
        self.serial = serial
        self.verbosity = verbosity

        ##################################################
        # Variables
        ##################################################
        self.bladerf_selection = bladerf_selection = str(instance) if serial == "" else serial
        self.bladerf_args = bladerf_args = "bladerf=" + bladerf_selection + ",buffers=" + str(num_buffers) + ",buflen=" + str(buflen) + ",num_xfers=" + str(num_xfers) + ",verbosity="+verbosity
        self.gui_rx_vga_gain = gui_rx_vga_gain = rx_vga_gain
        self.gui_rx_sample_rate = gui_rx_sample_rate = rx_sample_rate
        self.gui_rx_lna_gain = gui_rx_lna_gain = rx_lna_gain
        self.gui_rx_frequency = gui_rx_frequency = rx_frequency
        self.gui_rx_bandwidth = gui_rx_bandwidth = rx_bandwidth
        self.gui_dc_offset_q = gui_dc_offset_q = dc_offset_q
        self.gui_dc_offset_i = gui_dc_offset_i = dc_offset_i
        self.gui_bladerf_args = gui_bladerf_args = bladerf_args

        ##################################################
        # Blocks
        ##################################################
        self._gui_rx_vga_gain_range = Range(5, 60, 1, rx_vga_gain, 200)
        self._gui_rx_vga_gain_win = RangeWidget(self._gui_rx_vga_gain_range, self.set_gui_rx_vga_gain, "RX VGA1 + VGA2 Gain", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_rx_vga_gain_win, 0, 5, 1, 4)
        self._gui_rx_sample_rate_range = Range(1.5e6, 40e6, 500e3, rx_sample_rate, 200)
        self._gui_rx_sample_rate_win = RangeWidget(self._gui_rx_sample_rate_range, self.set_gui_rx_sample_rate, "Sample Rate", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_rx_sample_rate_win, 1, 0, 1, 2)
        self._gui_rx_lna_gain_options = (0, 3, 6, )
        self._gui_rx_lna_gain_labels = ("0 dB", "3 dB", "6 dB", )
        self._gui_rx_lna_gain_tool_bar = Qt.QToolBar(self)
        self._gui_rx_lna_gain_tool_bar.addWidget(Qt.QLabel("LNA Gain"+": "))
        self._gui_rx_lna_gain_combo_box = Qt.QComboBox()
        self._gui_rx_lna_gain_tool_bar.addWidget(self._gui_rx_lna_gain_combo_box)
        for label in self._gui_rx_lna_gain_labels: self._gui_rx_lna_gain_combo_box.addItem(label)
        self._gui_rx_lna_gain_callback = lambda i: Qt.QMetaObject.invokeMethod(self._gui_rx_lna_gain_combo_box, "setCurrentIndex", Qt.Q_ARG("int", self._gui_rx_lna_gain_options.index(i)))
        self._gui_rx_lna_gain_callback(self.gui_rx_lna_gain)
        self._gui_rx_lna_gain_combo_box.currentIndexChanged.connect(
                lambda i: self.set_gui_rx_lna_gain(self._gui_rx_lna_gain_options[i]))
        self.top_grid_layout.addWidget(self._gui_rx_lna_gain_tool_bar, 0, 9, 1, 1)
        self._gui_rx_frequency_range = Range(0, 3.8e9, 1e6, rx_frequency, 200)
        self._gui_rx_frequency_win = RangeWidget(self._gui_rx_frequency_range, self.set_gui_rx_frequency, "Frequency", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_rx_frequency_win, 0, 0, 1, 5)
        self._gui_rx_bandwidth_range = Range(1.5e6, 28e6, 0.5e6, rx_bandwidth, 200)
        self._gui_rx_bandwidth_win = RangeWidget(self._gui_rx_bandwidth_range, self.set_gui_rx_bandwidth, "Bandwidth", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_rx_bandwidth_win, 1, 2, 1, 2)
        self._gui_dc_offset_q_range = Range(-1.0, 1.0, (1.0 / 2048.0), dc_offset_q, 200)
        self._gui_dc_offset_q_win = RangeWidget(self._gui_dc_offset_q_range, self.set_gui_dc_offset_q, "Q DC Offset", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_dc_offset_q_win, 1, 6, 1, 2)
        self._gui_dc_offset_i_range = Range(-1.0, 1.0, (1.0 / 2048.0), dc_offset_i, 200)
        self._gui_dc_offset_i_win = RangeWidget(self._gui_dc_offset_i_range, self.set_gui_dc_offset_i, "I DC Offset", "counter_slider", float)
        self.top_grid_layout.addWidget(self._gui_dc_offset_i_win, 1, 4, 1, 2)
        self.qtgui_waterfall_sink_x_0 = qtgui.waterfall_sink_c(
                8192, #size
                firdes.WIN_BLACKMAN_hARRIS, #wintype
                gui_rx_frequency, #fc
                gui_rx_sample_rate, #bw
                "", #name
                1 #number of inputs
        )
        self.qtgui_waterfall_sink_x_0.set_update_time(0.10)
        self.qtgui_waterfall_sink_x_0.enable_grid(False)

        if not True:
          self.qtgui_waterfall_sink_x_0.disable_legend()

        if complex == type(float()):
          self.qtgui_waterfall_sink_x_0.set_plot_pos_half(not True)

        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        colors = [0, 0, 0, 0, 0,
                  0, 0, 0, 0, 0]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]
        for i in xrange(1):
            if len(labels[i]) == 0:
                self.qtgui_waterfall_sink_x_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_waterfall_sink_x_0.set_line_label(i, labels[i])
            self.qtgui_waterfall_sink_x_0.set_color_map(i, colors[i])
            self.qtgui_waterfall_sink_x_0.set_line_alpha(i, alphas[i])

        self.qtgui_waterfall_sink_x_0.set_intensity_range(-140, 10)

        self._qtgui_waterfall_sink_x_0_win = sip.wrapinstance(self.qtgui_waterfall_sink_x_0.pyqwidget(), Qt.QWidget)
        self.top_grid_layout.addWidget(self._qtgui_waterfall_sink_x_0_win, 2, 5, 5, 5)
        self.qtgui_time_sink_x_0 = qtgui.time_sink_c(
                8192, #size
                rx_sample_rate, #samp_rate
                "", #name
                1 #number of inputs
        )
        self.qtgui_time_sink_x_0.set_update_time(0.10)
        self.qtgui_time_sink_x_0.set_y_axis(-1, 1)

        self.qtgui_time_sink_x_0.set_y_label("Amplitude", "")

        self.qtgui_time_sink_x_0.enable_tags(-1, True)
        self.qtgui_time_sink_x_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, qtgui.TRIG_SLOPE_POS, 0.0, 0, 0, "")
        self.qtgui_time_sink_x_0.enable_autoscale(False)
        self.qtgui_time_sink_x_0.enable_grid(False)
        self.qtgui_time_sink_x_0.enable_control_panel(False)

        if not True:
          self.qtgui_time_sink_x_0.disable_legend()

        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        widths = [1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
                  "magenta", "yellow", "dark red", "dark green", "blue"]
        styles = [1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1]
        markers = [-1, -1, -1, -1, -1,
                   -1, -1, -1, -1, -1]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]

        for i in xrange(2*1):
            if len(labels[i]) == 0:
                if(i % 2 == 0):
                    self.qtgui_time_sink_x_0.set_line_label(i, "Re{{Data {0}}}".format(i/2))
                else:
                    self.qtgui_time_sink_x_0.set_line_label(i, "Im{{Data {0}}}".format(i/2))
            else:
                self.qtgui_time_sink_x_0.set_line_label(i, labels[i])
            self.qtgui_time_sink_x_0.set_line_width(i, widths[i])
            self.qtgui_time_sink_x_0.set_line_color(i, colors[i])
            self.qtgui_time_sink_x_0.set_line_style(i, styles[i])
            self.qtgui_time_sink_x_0.set_line_marker(i, markers[i])
            self.qtgui_time_sink_x_0.set_line_alpha(i, alphas[i])

        self._qtgui_time_sink_x_0_win = sip.wrapinstance(self.qtgui_time_sink_x_0.pyqwidget(), Qt.QWidget)
        self.top_grid_layout.addWidget(self._qtgui_time_sink_x_0_win, 7, 0, 3, 10)
        self.qtgui_freq_sink_x_0 = qtgui.freq_sink_c(
                8192, #size
                firdes.WIN_BLACKMAN_hARRIS, #wintype
                gui_rx_frequency, #fc
                gui_rx_sample_rate, #bw
                "", #name
                1 #number of inputs
        )
        self.qtgui_freq_sink_x_0.set_update_time(0.10)
        self.qtgui_freq_sink_x_0.set_y_axis(-140, 10)
        self.qtgui_freq_sink_x_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0.enable_grid(False)
        self.qtgui_freq_sink_x_0.set_fft_average(0.1)
        self.qtgui_freq_sink_x_0.enable_control_panel(False)

        if not True:
          self.qtgui_freq_sink_x_0.disable_legend()

        if complex == type(float()):
          self.qtgui_freq_sink_x_0.set_plot_pos_half(not True)

        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        widths = [1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
                  "magenta", "yellow", "dark red", "dark green", "dark blue"]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]
        for i in xrange(1):
            if len(labels[i]) == 0:
                self.qtgui_freq_sink_x_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0.set_line_alpha(i, alphas[i])

        self._qtgui_freq_sink_x_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0.pyqwidget(), Qt.QWidget)
        self.top_grid_layout.addWidget(self._qtgui_freq_sink_x_0_win, 2, 0, 5, 5)
        self.osmosdr_source_0 = osmosdr.source( args="numchan=" + str(1) + " " + bladerf_args )
        self.osmosdr_source_0.set_sample_rate(gui_rx_sample_rate)
        self.osmosdr_source_0.set_center_freq(gui_rx_frequency, 0)
        self.osmosdr_source_0.set_freq_corr(0, 0)
        self.osmosdr_source_0.set_dc_offset_mode(1, 0)
        self.osmosdr_source_0.set_iq_balance_mode(1, 0)
        self.osmosdr_source_0.set_gain_mode(False, 0)
        self.osmosdr_source_0.set_gain(gui_rx_lna_gain, 0)
        self.osmosdr_source_0.set_if_gain(0, 0)
        self.osmosdr_source_0.set_bb_gain(gui_rx_vga_gain, 0)
        self.osmosdr_source_0.set_antenna("", 0)
        self.osmosdr_source_0.set_bandwidth(gui_rx_bandwidth, 0)

        self._gui_bladerf_args_tool_bar = Qt.QToolBar(self)

        if None:
          self._gui_bladerf_args_formatter = None
        else:
          self._gui_bladerf_args_formatter = lambda x: x

        self._gui_bladerf_args_tool_bar.addWidget(Qt.QLabel("bladeRF arguments"+": "))
        self._gui_bladerf_args_label = Qt.QLabel(str(self._gui_bladerf_args_formatter(self.gui_bladerf_args)))
        self._gui_bladerf_args_tool_bar.addWidget(self._gui_bladerf_args_label)
        self.top_grid_layout.addWidget(self._gui_bladerf_args_tool_bar, 11, 0, 1, 10)

        self.blocks_float_to_complex_0 = blocks.float_to_complex(1)
        self.blocks_complex_to_float_0 = blocks.complex_to_float(1)
        self.blocks_add_const_vxx_0_0 = blocks.add_const_vff((gui_dc_offset_q, ))
        self.blocks_add_const_vxx_0 = blocks.add_const_vff((gui_dc_offset_i, ))

        ##################################################
        # Connections
        ##################################################
        self.connect((self.blocks_add_const_vxx_0, 0), (self.blocks_float_to_complex_0, 0))
        self.connect((self.blocks_add_const_vxx_0_0, 0), (self.blocks_float_to_complex_0, 1))
        self.connect((self.blocks_complex_to_float_0, 0), (self.blocks_add_const_vxx_0, 0))
        self.connect((self.blocks_complex_to_float_0, 1), (self.blocks_add_const_vxx_0_0, 0))
        self.connect((self.blocks_float_to_complex_0, 0), (self.qtgui_freq_sink_x_0, 0))
        self.connect((self.blocks_float_to_complex_0, 0), (self.qtgui_time_sink_x_0, 0))
        self.connect((self.blocks_float_to_complex_0, 0), (self.qtgui_waterfall_sink_x_0, 0))
        self.connect((self.osmosdr_source_0, 0), (self.blocks_complex_to_float_0, 0))

    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "bladeRF_rx")
        self.settings.setValue("geometry", self.saveGeometry())
        event.accept()

    def get_buflen(self):
        return self.buflen

    def set_buflen(self, buflen):
        self.buflen = buflen
        self.set_bladerf_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.num_buffers) + ",buflen=" + str(self.buflen) + ",num_xfers=" + str(self.num_xfers) + ",verbosity="+self.verbosity)

    def get_dc_offset_i(self):
        return self.dc_offset_i

    def set_dc_offset_i(self, dc_offset_i):
        self.dc_offset_i = dc_offset_i
        self.set_gui_dc_offset_i(self.dc_offset_i)

    def get_dc_offset_q(self):
        return self.dc_offset_q

    def set_dc_offset_q(self, dc_offset_q):
        self.dc_offset_q = dc_offset_q
        self.set_gui_dc_offset_q(self.dc_offset_q)

    def get_instance(self):
        return self.instance

    def set_instance(self, instance):
        self.instance = instance
        self.set_bladerf_selection(str(self.instance) if self.serial == "" else self.serial)

    def get_num_buffers(self):
        return self.num_buffers

    def set_num_buffers(self, num_buffers):
        self.num_buffers = num_buffers
        self.set_bladerf_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.num_buffers) + ",buflen=" + str(self.buflen) + ",num_xfers=" + str(self.num_xfers) + ",verbosity="+self.verbosity)

    def get_num_xfers(self):
        return self.num_xfers

    def set_num_xfers(self, num_xfers):
        self.num_xfers = num_xfers
        self.set_bladerf_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.num_buffers) + ",buflen=" + str(self.buflen) + ",num_xfers=" + str(self.num_xfers) + ",verbosity="+self.verbosity)

    def get_rx_bandwidth(self):
        return self.rx_bandwidth

    def set_rx_bandwidth(self, rx_bandwidth):
        self.rx_bandwidth = rx_bandwidth
        self.set_gui_rx_bandwidth(self.rx_bandwidth)

    def get_rx_frequency(self):
        return self.rx_frequency

    def set_rx_frequency(self, rx_frequency):
        self.rx_frequency = rx_frequency
        self.set_gui_rx_frequency(self.rx_frequency)

    def get_rx_lna_gain(self):
        return self.rx_lna_gain

    def set_rx_lna_gain(self, rx_lna_gain):
        self.rx_lna_gain = rx_lna_gain
        self.set_gui_rx_lna_gain(self.rx_lna_gain)

    def get_rx_sample_rate(self):
        return self.rx_sample_rate

    def set_rx_sample_rate(self, rx_sample_rate):
        self.rx_sample_rate = rx_sample_rate
        self.set_gui_rx_sample_rate(self.rx_sample_rate)
        self.qtgui_time_sink_x_0.set_samp_rate(self.rx_sample_rate)

    def get_rx_vga_gain(self):
        return self.rx_vga_gain

    def set_rx_vga_gain(self, rx_vga_gain):
        self.rx_vga_gain = rx_vga_gain
        self.set_gui_rx_vga_gain(self.rx_vga_gain)

    def get_serial(self):
        return self.serial

    def set_serial(self, serial):
        self.serial = serial
        self.set_bladerf_selection(str(self.instance) if self.serial == "" else self.serial)

    def get_verbosity(self):
        return self.verbosity

    def set_verbosity(self, verbosity):
        self.verbosity = verbosity
        self.set_bladerf_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.num_buffers) + ",buflen=" + str(self.buflen) + ",num_xfers=" + str(self.num_xfers) + ",verbosity="+self.verbosity)

    def get_bladerf_selection(self):
        return self.bladerf_selection

    def set_bladerf_selection(self, bladerf_selection):
        self.bladerf_selection = bladerf_selection
        self.set_bladerf_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.num_buffers) + ",buflen=" + str(self.buflen) + ",num_xfers=" + str(self.num_xfers) + ",verbosity="+self.verbosity)

    def get_bladerf_args(self):
        return self.bladerf_args

    def set_bladerf_args(self, bladerf_args):
        self.bladerf_args = bladerf_args
        self.set_gui_bladerf_args(self._gui_bladerf_args_formatter(self.bladerf_args))

    def get_gui_rx_vga_gain(self):
        return self.gui_rx_vga_gain

    def set_gui_rx_vga_gain(self, gui_rx_vga_gain):
        self.gui_rx_vga_gain = gui_rx_vga_gain
        self.osmosdr_source_0.set_bb_gain(self.gui_rx_vga_gain, 0)

    def get_gui_rx_sample_rate(self):
        return self.gui_rx_sample_rate

    def set_gui_rx_sample_rate(self, gui_rx_sample_rate):
        self.gui_rx_sample_rate = gui_rx_sample_rate
        self.osmosdr_source_0.set_sample_rate(self.gui_rx_sample_rate)
        self.qtgui_freq_sink_x_0.set_frequency_range(self.gui_rx_frequency, self.gui_rx_sample_rate)
        self.qtgui_waterfall_sink_x_0.set_frequency_range(self.gui_rx_frequency, self.gui_rx_sample_rate)

    def get_gui_rx_lna_gain(self):
        return self.gui_rx_lna_gain

    def set_gui_rx_lna_gain(self, gui_rx_lna_gain):
        self.gui_rx_lna_gain = gui_rx_lna_gain
        self._gui_rx_lna_gain_callback(self.gui_rx_lna_gain)
        self.osmosdr_source_0.set_gain(self.gui_rx_lna_gain, 0)

    def get_gui_rx_frequency(self):
        return self.gui_rx_frequency

    def set_gui_rx_frequency(self, gui_rx_frequency):
        self.gui_rx_frequency = gui_rx_frequency
        self.osmosdr_source_0.set_center_freq(self.gui_rx_frequency, 0)
        self.qtgui_freq_sink_x_0.set_frequency_range(self.gui_rx_frequency, self.gui_rx_sample_rate)
        self.qtgui_waterfall_sink_x_0.set_frequency_range(self.gui_rx_frequency, self.gui_rx_sample_rate)

    def get_gui_rx_bandwidth(self):
        return self.gui_rx_bandwidth

    def set_gui_rx_bandwidth(self, gui_rx_bandwidth):
        self.gui_rx_bandwidth = gui_rx_bandwidth
        self.osmosdr_source_0.set_bandwidth(self.gui_rx_bandwidth, 0)

    def get_gui_dc_offset_q(self):
        return self.gui_dc_offset_q

    def set_gui_dc_offset_q(self, gui_dc_offset_q):
        self.gui_dc_offset_q = gui_dc_offset_q
        self.blocks_add_const_vxx_0_0.set_k((self.gui_dc_offset_q, ))

    def get_gui_dc_offset_i(self):
        return self.gui_dc_offset_i

    def set_gui_dc_offset_i(self, gui_dc_offset_i):
        self.gui_dc_offset_i = gui_dc_offset_i
        self.blocks_add_const_vxx_0.set_k((self.gui_dc_offset_i, ))

    def get_gui_bladerf_args(self):
        return self.gui_bladerf_args

    def set_gui_bladerf_args(self, gui_bladerf_args):
        self.gui_bladerf_args = gui_bladerf_args
        Qt.QMetaObject.invokeMethod(self._gui_bladerf_args_label, "setText", Qt.Q_ARG("QString", str(self.gui_bladerf_args)))


if __name__ == '__main__':
    parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
    parser.add_option("", "--dc-offset-i", dest="dc_offset_i", type="eng_float", default=eng_notation.num_to_str(0),
        help="Set DC offset compensation for I channel [default=%default]")
    parser.add_option("", "--dc-offset-q", dest="dc_offset_q", type="eng_float", default=eng_notation.num_to_str(0),
        help="Set DC offset compensation for Q channel [default=%default]")
    parser.add_option("", "--instance", dest="instance", type="intx", default=0,
        help="Set 0-indexed device instance describing device to use. Ignored if a serial-number is provided. [default=%default]")
    parser.add_option("", "--num-buffers", dest="num_buffers", type="intx", default=16,
        help="Set Number of buffers to use [default=%default]")
    parser.add_option("", "--num-xfers", dest="num_xfers", type="intx", default=8,
        help="Set Number of maximum in-flight USB transfers. Should be <= (num-buffers / 2). [default=%default]")
    parser.add_option("-b", "--rx-bandwidth", dest="rx_bandwidth", type="eng_float", default=eng_notation.num_to_str(1.5e6),
        help="Set Bandwidth [default=%default]")
    parser.add_option("-f", "--rx-frequency", dest="rx_frequency", type="eng_float", default=eng_notation.num_to_str(915e6),
        help="Set Frequency [default=%default]")
    parser.add_option("-l", "--rx-lna-gain", dest="rx_lna_gain", type="intx", default=6,
        help="Set RX LNA Gain [default=%default]")
    parser.add_option("-s", "--rx-sample-rate", dest="rx_sample_rate", type="eng_float", default=eng_notation.num_to_str(3e6),
        help="Set Sample Rate [default=%default]")
    parser.add_option("-g", "--rx-vga-gain", dest="rx_vga_gain", type="intx", default=20,
        help="Set RX VGA1 + VGA2 Gain [default=%default]")
    parser.add_option("", "--serial", dest="serial", type="string", default="",
        help="Set bladeRF serial number [default=%default]")
    parser.add_option("", "--verbosity", dest="verbosity", type="string", default="info",
        help="Set libbladeRF log verbosity. Options are: verbose, debug, info, warning, error, critical, silent [default=%default]")
    (options, args) = parser.parse_args()
    from distutils.version import StrictVersion
    if StrictVersion(Qt.qVersion()) >= StrictVersion("4.5.0"):
        Qt.QApplication.setGraphicsSystem(gr.prefs().get_string('qtgui','style','raster'))
    qapp = Qt.QApplication(sys.argv)
    tb = bladeRF_rx(dc_offset_i=options.dc_offset_i, dc_offset_q=options.dc_offset_q, instance=options.instance, num_buffers=options.num_buffers, num_xfers=options.num_xfers, rx_bandwidth=options.rx_bandwidth, rx_frequency=options.rx_frequency, rx_lna_gain=options.rx_lna_gain, rx_sample_rate=options.rx_sample_rate, rx_vga_gain=options.rx_vga_gain, serial=options.serial, verbosity=options.verbosity)
    tb.start()
    tb.show()

    def quitting():
        tb.stop()
        tb.wait()
    qapp.connect(qapp, Qt.SIGNAL("aboutToQuit()"), quitting)
    qapp.exec_()
    tb = None  # to clean up Qt widgets
