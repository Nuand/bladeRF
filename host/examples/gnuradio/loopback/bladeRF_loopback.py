#!/usr/bin/env python2
##################################################
# GNU Radio Python Flow Graph
# Title: bladeRF loopback example
# Author: Nuand, LLC <bladeRF@nuand.com>
# Description: A simple flowgraph that demonstrates the usage of loopback modes.
# Generated: Sun Jan 17 21:26:48 2016
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
from gnuradio import digital
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import qtgui
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from gnuradio.qtgui import Range, RangeWidget
from optparse import OptionParser
import numpy
import osmosdr
import sip
import sys
import time


class bladeRF_loopback(gr.top_block, Qt.QWidget):

    def __init__(self, instance=0, loopback="bb_txvga1_rxlpf", rx_bandwidth=1.5e6, rx_buflen=4096, rx_frequency=915e6, rx_lna_gain=6, rx_num_buffers=16, rx_num_xfers=8, rx_sample_rate=3e6, rx_vga_gain=20, serial="", tx_bandwidth=1.5e6, tx_buflen=4096, tx_frequency=915e6, tx_num_buffers=16, tx_num_xfers=8, tx_sample_rate=3e6, tx_vga1=-18, tx_vga2=0, verbosity="info"):
        gr.top_block.__init__(self, "bladeRF loopback example")
        Qt.QWidget.__init__(self)
        self.setWindowTitle("bladeRF loopback example")
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

        self.settings = Qt.QSettings("GNU Radio", "bladeRF_loopback")
        self.restoreGeometry(self.settings.value("geometry").toByteArray())

        ##################################################
        # Parameters
        ##################################################
        self.instance = instance
        self.loopback = loopback
        self.rx_bandwidth = rx_bandwidth
        self.rx_buflen = rx_buflen
        self.rx_frequency = rx_frequency
        self.rx_lna_gain = rx_lna_gain
        self.rx_num_buffers = rx_num_buffers
        self.rx_num_xfers = rx_num_xfers
        self.rx_sample_rate = rx_sample_rate
        self.rx_vga_gain = rx_vga_gain
        self.serial = serial
        self.tx_bandwidth = tx_bandwidth
        self.tx_buflen = tx_buflen
        self.tx_frequency = tx_frequency
        self.tx_num_buffers = tx_num_buffers
        self.tx_num_xfers = tx_num_xfers
        self.tx_sample_rate = tx_sample_rate
        self.tx_vga1 = tx_vga1
        self.tx_vga2 = tx_vga2
        self.verbosity = verbosity

        ##################################################
        # Variables
        ##################################################
        self.bladerf_selection = bladerf_selection = str(instance) if serial == "" else serial
        self.bladerf_tx_args = bladerf_tx_args = "bladerf=" + bladerf_selection + ",buffers=" + str(tx_num_buffers) + ",buflen=" + str(tx_buflen) + ",num_xfers=" + str(tx_num_xfers) + ",verbosity="+verbosity
        self.bladerf_rx_args = bladerf_rx_args = "bladerf=" + bladerf_selection + ",loopback=" + loopback + ",buffers=" + str(rx_num_buffers) + ",buflen=" + str(rx_buflen) + ",num_xfers=" + str(rx_num_xfers) + ",verbosity="+verbosity
        self.gui_tx_vga2 = gui_tx_vga2 = tx_vga2
        self.gui_tx_vga1 = gui_tx_vga1 = tx_vga1
        self.gui_tx_sample_rate = gui_tx_sample_rate = tx_sample_rate
        self.gui_tx_frequency = gui_tx_frequency = tx_frequency
        self.gui_tx_bandwidth = gui_tx_bandwidth = tx_bandwidth
        self.gui_rx_vga_gain = gui_rx_vga_gain = rx_vga_gain
        self.gui_rx_sample_rate = gui_rx_sample_rate = rx_sample_rate
        self.gui_rx_lna_gain = gui_rx_lna_gain = rx_lna_gain
        self.gui_rx_frequency = gui_rx_frequency = rx_frequency
        self.gui_rx_bandwidth = gui_rx_bandwidth = rx_bandwidth
        self.gui_bladerf_tx_args = gui_bladerf_tx_args = bladerf_tx_args
        self.gui_bladerf_rx_args = gui_bladerf_rx_args = bladerf_rx_args

        ##################################################
        # Blocks
        ##################################################
        self.tabs = Qt.QTabWidget()
        self.tabs_widget_0 = Qt.QWidget()
        self.tabs_layout_0 = Qt.QBoxLayout(Qt.QBoxLayout.TopToBottom, self.tabs_widget_0)
        self.tabs_grid_layout_0 = Qt.QGridLayout()
        self.tabs_layout_0.addLayout(self.tabs_grid_layout_0)
        self.tabs.addTab(self.tabs_widget_0, "RX")
        self.tabs_widget_1 = Qt.QWidget()
        self.tabs_layout_1 = Qt.QBoxLayout(Qt.QBoxLayout.TopToBottom, self.tabs_widget_1)
        self.tabs_grid_layout_1 = Qt.QGridLayout()
        self.tabs_layout_1.addLayout(self.tabs_grid_layout_1)
        self.tabs.addTab(self.tabs_widget_1, "TX")
        self.top_layout.addWidget(self.tabs)
        self._gui_tx_vga2_range = Range(0, 25, 1, tx_vga2, 200)
        self._gui_tx_vga2_win = RangeWidget(self._gui_tx_vga2_range, self.set_gui_tx_vga2, "VGA2 Gain", "counter_slider", float)
        self.tabs_grid_layout_1 .addWidget(self._gui_tx_vga2_win,  0, 7, 1, 2)
        self._gui_tx_vga1_range = Range(-35, -4, 1, tx_vga1, 200)
        self._gui_tx_vga1_win = RangeWidget(self._gui_tx_vga1_range, self.set_gui_tx_vga1, "VGA1 Gain", "counter_slider", float)
        self.tabs_grid_layout_1 .addWidget(self._gui_tx_vga1_win,  0, 5, 1, 2)
        self._gui_tx_sample_rate_range = Range(1.5e6, 40e6, 500e3, tx_sample_rate, 200)
        self._gui_tx_sample_rate_win = RangeWidget(self._gui_tx_sample_rate_range, self.set_gui_tx_sample_rate, "Sample Rate", "counter_slider", float)
        self.tabs_grid_layout_1 .addWidget(self._gui_tx_sample_rate_win,  1, 0, 1, 2)
        self._gui_tx_frequency_range = Range(0, 3.8e9, 1e6, tx_frequency, 200)
        self._gui_tx_frequency_win = RangeWidget(self._gui_tx_frequency_range, self.set_gui_tx_frequency, "Frequency", "counter_slider", float)
        self.tabs_grid_layout_1 .addWidget(self._gui_tx_frequency_win,  0, 0, 1, 5)
        self._gui_tx_bandwidth_range = Range(1.5e6, 28e6, 0.5e6, tx_bandwidth, 200)
        self._gui_tx_bandwidth_win = RangeWidget(self._gui_tx_bandwidth_range, self.set_gui_tx_bandwidth, "Bandwidth", "counter_slider", float)
        self.tabs_grid_layout_1 .addWidget(self._gui_tx_bandwidth_win,  1, 2, 1, 2)
        self._gui_rx_vga_gain_range = Range(5, 60, 1, rx_vga_gain, 200)
        self._gui_rx_vga_gain_win = RangeWidget(self._gui_rx_vga_gain_range, self.set_gui_rx_vga_gain, "RX VGA1 + VGA2 Gain", "counter_slider", float)
        self.tabs_grid_layout_0 .addWidget(self._gui_rx_vga_gain_win,  0, 5, 1, 4)
        self._gui_rx_sample_rate_range = Range(1.5e6, 40e6, 500e3, rx_sample_rate, 200)
        self._gui_rx_sample_rate_win = RangeWidget(self._gui_rx_sample_rate_range, self.set_gui_rx_sample_rate, "Sample Rate", "counter_slider", float)
        self.tabs_grid_layout_0 .addWidget(self._gui_rx_sample_rate_win,  1, 0, 1, 2)
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
        self.tabs_grid_layout_0 .addWidget(self._gui_rx_lna_gain_tool_bar,  0, 9, 1, 1)
        self._gui_rx_frequency_range = Range(0, 3.8e9, 1e6, rx_frequency, 200)
        self._gui_rx_frequency_win = RangeWidget(self._gui_rx_frequency_range, self.set_gui_rx_frequency, "Frequency", "counter_slider", float)
        self.tabs_grid_layout_0 .addWidget(self._gui_rx_frequency_win,  0, 0, 1, 5)
        self._gui_rx_bandwidth_range = Range(1.5e6, 28e6, 0.5e6, rx_bandwidth, 200)
        self._gui_rx_bandwidth_win = RangeWidget(self._gui_rx_bandwidth_range, self.set_gui_rx_bandwidth, "Bandwidth", "counter_slider", float)
        self.tabs_grid_layout_0 .addWidget(self._gui_rx_bandwidth_win,  1, 2, 1, 2)
        self.qtgui_waterfall_sink_x_0_0 = qtgui.waterfall_sink_c(
        	8192, #size
        	firdes.WIN_BLACKMAN_hARRIS, #wintype
        	gui_tx_frequency, #fc
        	gui_tx_sample_rate, #bw
        	"", #name
                1 #number of inputs
        )
        self.qtgui_waterfall_sink_x_0_0.set_update_time(0.10)
        self.qtgui_waterfall_sink_x_0_0.enable_grid(False)
        
        if not True:
          self.qtgui_waterfall_sink_x_0_0.disable_legend()
        
        if complex == type(float()):
          self.qtgui_waterfall_sink_x_0_0.set_plot_pos_half(not True)
        
        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        colors = [0, 0, 0, 0, 0,
                  0, 0, 0, 0, 0]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]
        for i in xrange(1):
            if len(labels[i]) == 0:
                self.qtgui_waterfall_sink_x_0_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_waterfall_sink_x_0_0.set_line_label(i, labels[i])
            self.qtgui_waterfall_sink_x_0_0.set_color_map(i, colors[i])
            self.qtgui_waterfall_sink_x_0_0.set_line_alpha(i, alphas[i])
        
        self.qtgui_waterfall_sink_x_0_0.set_intensity_range(-140, 10)
        
        self._qtgui_waterfall_sink_x_0_0_win = sip.wrapinstance(self.qtgui_waterfall_sink_x_0_0.pyqwidget(), Qt.QWidget)
        self.tabs_grid_layout_1 .addWidget(self._qtgui_waterfall_sink_x_0_0_win,  2, 5, 5, 5)
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
        self.tabs_grid_layout_0 .addWidget(self._qtgui_waterfall_sink_x_0_win,  2, 5, 5, 5)
        self.qtgui_time_sink_x_0_0 = qtgui.time_sink_c(
        	8192, #size
        	tx_sample_rate, #samp_rate
        	"", #name
        	1 #number of inputs
        )
        self.qtgui_time_sink_x_0_0.set_update_time(0.10)
        self.qtgui_time_sink_x_0_0.set_y_axis(-1, 1)
        
        self.qtgui_time_sink_x_0_0.set_y_label("Amplitude", "")
        
        self.qtgui_time_sink_x_0_0.enable_tags(-1, True)
        self.qtgui_time_sink_x_0_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, qtgui.TRIG_SLOPE_POS, 0.0, 0, 0, "")
        self.qtgui_time_sink_x_0_0.enable_autoscale(False)
        self.qtgui_time_sink_x_0_0.enable_grid(False)
        self.qtgui_time_sink_x_0_0.enable_control_panel(False)
        
        if not True:
          self.qtgui_time_sink_x_0_0.disable_legend()
        
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
                    self.qtgui_time_sink_x_0_0.set_line_label(i, "Re{{Data {0}}}".format(i/2))
                else:
                    self.qtgui_time_sink_x_0_0.set_line_label(i, "Im{{Data {0}}}".format(i/2))
            else:
                self.qtgui_time_sink_x_0_0.set_line_label(i, labels[i])
            self.qtgui_time_sink_x_0_0.set_line_width(i, widths[i])
            self.qtgui_time_sink_x_0_0.set_line_color(i, colors[i])
            self.qtgui_time_sink_x_0_0.set_line_style(i, styles[i])
            self.qtgui_time_sink_x_0_0.set_line_marker(i, markers[i])
            self.qtgui_time_sink_x_0_0.set_line_alpha(i, alphas[i])
        
        self._qtgui_time_sink_x_0_0_win = sip.wrapinstance(self.qtgui_time_sink_x_0_0.pyqwidget(), Qt.QWidget)
        self.tabs_grid_layout_1 .addWidget(self._qtgui_time_sink_x_0_0_win,  7, 0, 3, 10)
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
        self.tabs_grid_layout_0 .addWidget(self._qtgui_time_sink_x_0_win,  7, 0, 3, 10)
        self.qtgui_freq_sink_x_0_0 = qtgui.freq_sink_c(
        	8192, #size
        	firdes.WIN_BLACKMAN_hARRIS, #wintype
        	gui_tx_frequency, #fc
        	gui_tx_sample_rate, #bw
        	"", #name
        	1 #number of inputs
        )
        self.qtgui_freq_sink_x_0_0.set_update_time(0.10)
        self.qtgui_freq_sink_x_0_0.set_y_axis(-140, 10)
        self.qtgui_freq_sink_x_0_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0_0.enable_grid(False)
        self.qtgui_freq_sink_x_0_0.set_fft_average(0.1)
        self.qtgui_freq_sink_x_0_0.enable_control_panel(False)
        
        if not True:
          self.qtgui_freq_sink_x_0_0.disable_legend()
        
        if complex == type(float()):
          self.qtgui_freq_sink_x_0_0.set_plot_pos_half(not True)
        
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
                self.qtgui_freq_sink_x_0_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0_0.set_line_alpha(i, alphas[i])
        
        self._qtgui_freq_sink_x_0_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0_0.pyqwidget(), Qt.QWidget)
        self.tabs_grid_layout_1 .addWidget(self._qtgui_freq_sink_x_0_0_win,  2, 0, 5, 5)
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
        self.tabs_grid_layout_0 .addWidget(self._qtgui_freq_sink_x_0_win,  2, 0, 5, 5)
        self.osmosdr_source_0 = osmosdr.source( args="numchan=" + str(1) + " " + bladerf_rx_args )
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
          
        self.osmosdr_sink_0 = osmosdr.sink( args="numchan=" + str(1) + " " + bladerf_tx_args )
        self.osmosdr_sink_0.set_sample_rate(gui_tx_sample_rate)
        self.osmosdr_sink_0.set_center_freq(gui_tx_frequency, 0)
        self.osmosdr_sink_0.set_freq_corr(0, 0)
        self.osmosdr_sink_0.set_gain(gui_tx_vga2, 0)
        self.osmosdr_sink_0.set_if_gain(0, 0)
        self.osmosdr_sink_0.set_bb_gain(gui_tx_vga1, 0)
        self.osmosdr_sink_0.set_antenna("", 0)
        self.osmosdr_sink_0.set_bandwidth(gui_tx_bandwidth, 0)
          
        self._gui_bladerf_tx_args_tool_bar = Qt.QToolBar(self)
        
        if None:
          self._gui_bladerf_tx_args_formatter = None
        else:
          self._gui_bladerf_tx_args_formatter = lambda x: x
        
        self._gui_bladerf_tx_args_tool_bar.addWidget(Qt.QLabel("bladeRF TX arguments"+": "))
        self._gui_bladerf_tx_args_label = Qt.QLabel(str(self._gui_bladerf_tx_args_formatter(self.gui_bladerf_tx_args)))
        self._gui_bladerf_tx_args_tool_bar.addWidget(self._gui_bladerf_tx_args_label)
        self.tabs_grid_layout_1.addWidget(self._gui_bladerf_tx_args_tool_bar,  11, 0, 1, 10)
          
        self._gui_bladerf_rx_args_tool_bar = Qt.QToolBar(self)
        
        if None:
          self._gui_bladerf_rx_args_formatter = None
        else:
          self._gui_bladerf_rx_args_formatter = lambda x: x
        
        self._gui_bladerf_rx_args_tool_bar.addWidget(Qt.QLabel("bladeRF RX arguments"+": "))
        self._gui_bladerf_rx_args_label = Qt.QLabel(str(self._gui_bladerf_rx_args_formatter(self.gui_bladerf_rx_args)))
        self._gui_bladerf_rx_args_tool_bar.addWidget(self._gui_bladerf_rx_args_label)
        self.tabs_grid_layout_0 .addWidget(self._gui_bladerf_rx_args_tool_bar,  11, 0, 1, 10)
          
        self.digital_psk_mod_0 = digital.psk.psk_mod(
          constellation_points=4,
          mod_code="none",
          differential=True,
          samples_per_symbol=8,
          excess_bw=0.35,
          verbose=False,
          log=False,
          )
        self.blocks_multiply_const_vxx_0 = blocks.multiply_const_vcc((0.3, ))
        self.analog_random_source_x_0 = blocks.vector_source_b(map(int, numpy.random.randint(0, 255, 1000)), True)

        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_random_source_x_0, 0), (self.digital_psk_mod_0, 0))    
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.osmosdr_sink_0, 0))    
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.qtgui_freq_sink_x_0_0, 0))    
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.qtgui_time_sink_x_0_0, 0))    
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.qtgui_waterfall_sink_x_0_0, 0))    
        self.connect((self.digital_psk_mod_0, 0), (self.blocks_multiply_const_vxx_0, 0))    
        self.connect((self.osmosdr_source_0, 0), (self.qtgui_freq_sink_x_0, 0))    
        self.connect((self.osmosdr_source_0, 0), (self.qtgui_time_sink_x_0, 0))    
        self.connect((self.osmosdr_source_0, 0), (self.qtgui_waterfall_sink_x_0, 0))    

    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "bladeRF_loopback")
        self.settings.setValue("geometry", self.saveGeometry())
        event.accept()

    def get_instance(self):
        return self.instance

    def set_instance(self, instance):
        self.instance = instance
        self.set_bladerf_selection(str(self.instance) if self.serial == "" else self.serial)

    def get_loopback(self):
        return self.loopback

    def set_loopback(self, loopback):
        self.loopback = loopback
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)

    def get_rx_bandwidth(self):
        return self.rx_bandwidth

    def set_rx_bandwidth(self, rx_bandwidth):
        self.rx_bandwidth = rx_bandwidth
        self.set_gui_rx_bandwidth(self.rx_bandwidth)

    def get_rx_buflen(self):
        return self.rx_buflen

    def set_rx_buflen(self, rx_buflen):
        self.rx_buflen = rx_buflen
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)

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

    def get_rx_num_buffers(self):
        return self.rx_num_buffers

    def set_rx_num_buffers(self, rx_num_buffers):
        self.rx_num_buffers = rx_num_buffers
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)

    def get_rx_num_xfers(self):
        return self.rx_num_xfers

    def set_rx_num_xfers(self, rx_num_xfers):
        self.rx_num_xfers = rx_num_xfers
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)

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

    def get_tx_bandwidth(self):
        return self.tx_bandwidth

    def set_tx_bandwidth(self, tx_bandwidth):
        self.tx_bandwidth = tx_bandwidth
        self.set_gui_tx_bandwidth(self.tx_bandwidth)

    def get_tx_buflen(self):
        return self.tx_buflen

    def set_tx_buflen(self, tx_buflen):
        self.tx_buflen = tx_buflen
        self.set_bladerf_tx_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.tx_num_buffers) + ",buflen=" + str(self.tx_buflen) + ",num_xfers=" + str(self.tx_num_xfers) + ",verbosity="+self.verbosity)

    def get_tx_frequency(self):
        return self.tx_frequency

    def set_tx_frequency(self, tx_frequency):
        self.tx_frequency = tx_frequency
        self.set_gui_tx_frequency(self.tx_frequency)

    def get_tx_num_buffers(self):
        return self.tx_num_buffers

    def set_tx_num_buffers(self, tx_num_buffers):
        self.tx_num_buffers = tx_num_buffers
        self.set_bladerf_tx_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.tx_num_buffers) + ",buflen=" + str(self.tx_buflen) + ",num_xfers=" + str(self.tx_num_xfers) + ",verbosity="+self.verbosity)

    def get_tx_num_xfers(self):
        return self.tx_num_xfers

    def set_tx_num_xfers(self, tx_num_xfers):
        self.tx_num_xfers = tx_num_xfers
        self.set_bladerf_tx_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.tx_num_buffers) + ",buflen=" + str(self.tx_buflen) + ",num_xfers=" + str(self.tx_num_xfers) + ",verbosity="+self.verbosity)

    def get_tx_sample_rate(self):
        return self.tx_sample_rate

    def set_tx_sample_rate(self, tx_sample_rate):
        self.tx_sample_rate = tx_sample_rate
        self.set_gui_tx_sample_rate(self.tx_sample_rate)
        self.qtgui_time_sink_x_0_0.set_samp_rate(self.tx_sample_rate)

    def get_tx_vga1(self):
        return self.tx_vga1

    def set_tx_vga1(self, tx_vga1):
        self.tx_vga1 = tx_vga1
        self.set_gui_tx_vga1(self.tx_vga1)

    def get_tx_vga2(self):
        return self.tx_vga2

    def set_tx_vga2(self, tx_vga2):
        self.tx_vga2 = tx_vga2
        self.set_gui_tx_vga2(self.tx_vga2)

    def get_verbosity(self):
        return self.verbosity

    def set_verbosity(self, verbosity):
        self.verbosity = verbosity
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)
        self.set_bladerf_tx_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.tx_num_buffers) + ",buflen=" + str(self.tx_buflen) + ",num_xfers=" + str(self.tx_num_xfers) + ",verbosity="+self.verbosity)

    def get_bladerf_selection(self):
        return self.bladerf_selection

    def set_bladerf_selection(self, bladerf_selection):
        self.bladerf_selection = bladerf_selection
        self.set_bladerf_rx_args("bladerf=" + self.bladerf_selection + ",loopback=" + self.loopback + ",buffers=" + str(self.rx_num_buffers) + ",buflen=" + str(self.rx_buflen) + ",num_xfers=" + str(self.rx_num_xfers) + ",verbosity="+self.verbosity)
        self.set_bladerf_tx_args("bladerf=" + self.bladerf_selection + ",buffers=" + str(self.tx_num_buffers) + ",buflen=" + str(self.tx_buflen) + ",num_xfers=" + str(self.tx_num_xfers) + ",verbosity="+self.verbosity)

    def get_bladerf_tx_args(self):
        return self.bladerf_tx_args

    def set_bladerf_tx_args(self, bladerf_tx_args):
        self.bladerf_tx_args = bladerf_tx_args
        self.set_gui_bladerf_tx_args(self._gui_bladerf_tx_args_formatter(self.bladerf_tx_args))

    def get_bladerf_rx_args(self):
        return self.bladerf_rx_args

    def set_bladerf_rx_args(self, bladerf_rx_args):
        self.bladerf_rx_args = bladerf_rx_args
        self.set_gui_bladerf_rx_args(self._gui_bladerf_rx_args_formatter(self.bladerf_rx_args))

    def get_gui_tx_vga2(self):
        return self.gui_tx_vga2

    def set_gui_tx_vga2(self, gui_tx_vga2):
        self.gui_tx_vga2 = gui_tx_vga2
        self.osmosdr_sink_0.set_gain(self.gui_tx_vga2, 0)

    def get_gui_tx_vga1(self):
        return self.gui_tx_vga1

    def set_gui_tx_vga1(self, gui_tx_vga1):
        self.gui_tx_vga1 = gui_tx_vga1
        self.osmosdr_sink_0.set_bb_gain(self.gui_tx_vga1, 0)

    def get_gui_tx_sample_rate(self):
        return self.gui_tx_sample_rate

    def set_gui_tx_sample_rate(self, gui_tx_sample_rate):
        self.gui_tx_sample_rate = gui_tx_sample_rate
        self.osmosdr_sink_0.set_sample_rate(self.gui_tx_sample_rate)
        self.qtgui_freq_sink_x_0_0.set_frequency_range(self.gui_tx_frequency, self.gui_tx_sample_rate)
        self.qtgui_waterfall_sink_x_0_0.set_frequency_range(self.gui_tx_frequency, self.gui_tx_sample_rate)

    def get_gui_tx_frequency(self):
        return self.gui_tx_frequency

    def set_gui_tx_frequency(self, gui_tx_frequency):
        self.gui_tx_frequency = gui_tx_frequency
        self.osmosdr_sink_0.set_center_freq(self.gui_tx_frequency, 0)
        self.qtgui_freq_sink_x_0_0.set_frequency_range(self.gui_tx_frequency, self.gui_tx_sample_rate)
        self.qtgui_waterfall_sink_x_0_0.set_frequency_range(self.gui_tx_frequency, self.gui_tx_sample_rate)

    def get_gui_tx_bandwidth(self):
        return self.gui_tx_bandwidth

    def set_gui_tx_bandwidth(self, gui_tx_bandwidth):
        self.gui_tx_bandwidth = gui_tx_bandwidth
        self.osmosdr_sink_0.set_bandwidth(self.gui_tx_bandwidth, 0)

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

    def get_gui_bladerf_tx_args(self):
        return self.gui_bladerf_tx_args

    def set_gui_bladerf_tx_args(self, gui_bladerf_tx_args):
        self.gui_bladerf_tx_args = gui_bladerf_tx_args
        Qt.QMetaObject.invokeMethod(self._gui_bladerf_tx_args_label, "setText", Qt.Q_ARG("QString", str(self.gui_bladerf_tx_args)))

    def get_gui_bladerf_rx_args(self):
        return self.gui_bladerf_rx_args

    def set_gui_bladerf_rx_args(self, gui_bladerf_rx_args):
        self.gui_bladerf_rx_args = gui_bladerf_rx_args
        Qt.QMetaObject.invokeMethod(self._gui_bladerf_rx_args_label, "setText", Qt.Q_ARG("QString", str(self.gui_bladerf_rx_args)))


if __name__ == '__main__':
    parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
    parser.add_option("", "--instance", dest="instance", type="intx", default=0,
        help="Set 0-indexed device instance describing device to use. Ignored if a serial-number is provided. [default=%default]")
    parser.add_option("-l", "--loopback", dest="loopback", type="string", default="bb_txvga1_rxlpf",
        help="Set Loopback mode [default=%default]")
    parser.add_option("", "--rx-bandwidth", dest="rx_bandwidth", type="eng_float", default=eng_notation.num_to_str(1.5e6),
        help="Set Bandwidth [default=%default]")
    parser.add_option("", "--rx-frequency", dest="rx_frequency", type="eng_float", default=eng_notation.num_to_str(915e6),
        help="Set Frequency [default=%default]")
    parser.add_option("", "--rx-lna-gain", dest="rx_lna_gain", type="intx", default=6,
        help="Set RX LNA Gain [default=%default]")
    parser.add_option("", "--rx-num-buffers", dest="rx_num_buffers", type="intx", default=16,
        help="Set Number of RX buffers to use [default=%default]")
    parser.add_option("", "--rx-num-xfers", dest="rx_num_xfers", type="intx", default=8,
        help="Set Number of maximum in-flight RX USB transfers. Should be <= (num-buffers / 2). [default=%default]")
    parser.add_option("", "--rx-sample-rate", dest="rx_sample_rate", type="eng_float", default=eng_notation.num_to_str(3e6),
        help="Set Sample Rate [default=%default]")
    parser.add_option("", "--rx-vga-gain", dest="rx_vga_gain", type="intx", default=20,
        help="Set RX VGA1 + VGA2 Gain [default=%default]")
    parser.add_option("", "--serial", dest="serial", type="string", default="",
        help="Set bladeRF serial number [default=%default]")
    parser.add_option("", "--tx-bandwidth", dest="tx_bandwidth", type="eng_float", default=eng_notation.num_to_str(1.5e6),
        help="Set Bandwidth [default=%default]")
    parser.add_option("", "--tx-frequency", dest="tx_frequency", type="eng_float", default=eng_notation.num_to_str(915e6),
        help="Set Frequency [default=%default]")
    parser.add_option("", "--tx-num-buffers", dest="tx_num_buffers", type="intx", default=16,
        help="Set Number of TX buffers to use [default=%default]")
    parser.add_option("", "--tx-num-xfers", dest="tx_num_xfers", type="intx", default=8,
        help="Set Number of maximum in-flight TX USB transfers. Should be <= (num-buffers / 2). [default=%default]")
    parser.add_option("", "--tx-sample-rate", dest="tx_sample_rate", type="eng_float", default=eng_notation.num_to_str(3e6),
        help="Set Sample Rate [default=%default]")
    parser.add_option("", "--tx-vga1", dest="tx_vga1", type="intx", default=-18,
        help="Set TX VGA1 Gain [default=%default]")
    parser.add_option("", "--tx-vga2", dest="tx_vga2", type="intx", default=0,
        help="Set TX VGA2 Gain [default=%default]")
    parser.add_option("", "--verbosity", dest="verbosity", type="string", default="info",
        help="Set libbladeRF log verbosity. Options are: verbose, debug, info, warning, error, critical, silent [default=%default]")
    (options, args) = parser.parse_args()
    from distutils.version import StrictVersion
    if StrictVersion(Qt.qVersion()) >= StrictVersion("4.5.0"):
        Qt.QApplication.setGraphicsSystem(gr.prefs().get_string('qtgui','style','raster'))
    qapp = Qt.QApplication(sys.argv)
    tb = bladeRF_loopback(instance=options.instance, loopback=options.loopback, rx_bandwidth=options.rx_bandwidth, rx_frequency=options.rx_frequency, rx_lna_gain=options.rx_lna_gain, rx_num_buffers=options.rx_num_buffers, rx_num_xfers=options.rx_num_xfers, rx_sample_rate=options.rx_sample_rate, rx_vga_gain=options.rx_vga_gain, serial=options.serial, tx_bandwidth=options.tx_bandwidth, tx_frequency=options.tx_frequency, tx_num_buffers=options.tx_num_buffers, tx_num_xfers=options.tx_num_xfers, tx_sample_rate=options.tx_sample_rate, tx_vga1=options.tx_vga1, tx_vga2=options.tx_vga2, verbosity=options.verbosity)
    tb.start()
    tb.show()

    def quitting():
        tb.stop()
        tb.wait()
    qapp.connect(qapp, Qt.SIGNAL("aboutToQuit()"), quitting)
    qapp.exec_()
    tb = None  # to clean up Qt widgets
