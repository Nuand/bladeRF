#!/usr/bin/env python3
# This file is part of the bladeRF project:
#   http://www.github.com/nuand/bladeRF
#
# Copyright (C) 2023 Nuand LLC
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
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import scipy.stats as st
import os
import sys
import argparse
import subprocess
import json

def close_figure(event):
    if event.key == 'escape':
        plt.close()

def edge_detector(signal, rising_threshold, falling_threshold, debounce_time):
    state = 'low'
    debounce_downcount = debounce_time
    rising = np.zeros_like(signal, dtype=bool)
    falling = np.zeros_like(signal, dtype=bool)

    for i in range(len(signal)):
        if (state == 'low' and signal[i] > rising_threshold):
            rising[i] = True
            last_state = state
            state = 'debounce'

        if (state == 'debounce'):
            debounce_downcount -= 1
            if (debounce_downcount == 0):
                debounce_downcount = debounce_time
                state = 'high' if (last_state == 'low') else 'low'

        if (state == 'high' and signal[i] < falling_threshold):
            falling[i] = True
            last_state = state
            state = 'debounce'

    return rising, falling

###############################################################
# Initialize Parameters
###############################################################
fill = 90
burst = 10e3
period = 50e3
iterations = 10
threshold = 2e6
cycles_to_debounce = 50
devarg_tx = ""
devarg_rx = ""
samp_rate_arg = ""
devarg_verbosity = ""
devarg_tx_gain = ""
devarg_rx_gain = ""
print_stats = False

parser = argparse.ArgumentParser(
    description='TXRX hardware loop timestamp validation',
    allow_abbrev=False,
    add_help=True
)
parser.add_argument('-f', '--fill', type=float, help='fill (%%)')
parser.add_argument('-b', '--burst', type=int, help='burst length (in samples)')
parser.add_argument('-sr', '--samprate', type=int, help='Sample Rate (Hz)', default=1e6)
parser.add_argument('-p', '--period', type=int, help='period length (in samples)')
parser.add_argument('-i', '--iterations', type=int, help='number of pulses')
parser.add_argument('-t', '--threshold', type=int, help='edge count power threshold')
parser.add_argument('-ts', '--threshold-spread', type=int, help='threshold spread (rising = threshold + spread, falling = threshold - spread)', default=int(1e6))
parser.add_argument('-tx', '--txdev', type=str, help='TX device string')
parser.add_argument('-rx', '--rxdev', type=str, help='RX device string')
parser.add_argument('-g', '--tx-gain', type=int, help='TX gain (unified gain mode)')
parser.add_argument('-G', '--rx-gain', type=int, help='RX gain (unified gain mode, MGC)')
parser.add_argument('-v', '--verbosity', type=str, help='bladeRF log level', default="info")
parser.add_argument('-c', '--compare', help='RF loopback compare', action="store_true", default=False)
parser.add_argument('-l', '--loop', help='Enable RX device', action="store_false", default=True, dest="just_tx")
parser.add_argument('-s', '--stats', help='print edge statistics', action="store_true", default=False)
parser.add_argument('-o', '--output', type=str, help='output stats to the specified filename, in JSON format')
parser.add_argument('-ng', '--no-gui', help='No GUI', action="store_false", default=True, dest="gui")
parser.add_argument('-e', '--enforce', type=float, help='enforce results and exit code appropriately (in samples, lower is more strict, less than or equal)', default=False)
parser.add_argument('--save-png', type=str, help='Path to save the plot output as a PNG file. When set the gui will not be shown. Use --png-width and --png-height to control size.')
parser.add_argument('--png-width', type=int, help='Width of saved PNG in pixels (default: 800 for single plot, 1600 for dual plot)', default=None)
parser.add_argument('--png-height', type=int, help='Height of saved PNG in pixels (default: 600)', default=None)

args = parser.parse_args()

if args.fill:
    fill = args.fill
if args.burst:
    burst = args.burst
if args.period:
    period = args.period
if args.iterations:
    iterations = args.iterations
if args.threshold:
    threshold = args.threshold
if args.txdev:
    dev_tx = args.txdev
    devarg_tx = f"-t {dev_tx}"
if args.verbosity:
    devarg_verbosity = f"-v {args.verbosity}"
if args.rxdev:
    dev_rx = args.rxdev
    devarg_rx = f"-r {dev_rx}"
if args.stats:
    print_stats = True
if args.samprate:
    samp_rate_arg = f"-s {args.samprate}"
if args.tx_gain:
    devarg_tx_gain = f"-g {args.tx_gain}"
if args.rx_gain:
    devarg_rx_gain = f"-G {args.rx_gain}"

plot_count = 2 if not args.just_tx and args.compare else 1
first_label = "TX" if args.just_tx else "RX"
first_file = "compare.csv" if args.just_tx else "samples.csv"
jout = {}
jout["settings"] = {}
jout["settings"]["python_args"] = " ".join(sys.argv[1:])
test_pass = True

threshold_spread  = args.threshold_spread
rising_threshold  = threshold + threshold_spread
falling_threshold = threshold - threshold_spread

################################################################
# Generate Pulse
################################################################
binary_file = 'libbladeRF_test_txrx_hwloop'
output_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(output_dir)
proc = subprocess.run(f"./{binary_file} --fill {fill} --burst {burst} --period {period} "
                      f"--iterations {iterations} {devarg_tx} {devarg_rx} {args.compare * '-c'} {(not args.just_tx) * '-l'} "
                      f"{devarg_verbosity} {samp_rate_arg} {devarg_tx_gain} {devarg_rx_gain}", shell=True)

if proc.returncode != 0:
    print("Failed to run hwloop binary")
    exit(1)

if not args.compare and args.just_tx:
    print("No output written for python to analyze. Please use a combination of -c and -l to display something useful.")
    exit(1)

################################################################
# TX or RX Data Analysis
################################################################
data = pd.read_csv(first_file)

I = data['I'].to_numpy()
Q = data['Q'].to_numpy()
power = I**2 + Q**2
num_samples = range(len(I))

# Calculate figure size for custom PNG dimensions or GUI display
if args.save_png:
    # For PNG output, calculate figsize based on desired pixel dimensions
    default_width = 1600 if plot_count == 2 else 800
    default_height = 600
    
    png_width = args.png_width if args.png_width else default_width
    png_height = args.png_height if args.png_height else default_height
    
    # Use DPI of 100 to make calculations simple (pixels = inches * 100)
    dpi = 100
    figsize = (png_width/dpi, png_height/dpi)
else:
    # For GUI display, use the original sizes
    figsize = (16, 6) if plot_count == 2 else (8, 6)
    dpi = 100

if args.gui or args.save_png:
    if plot_count == 2:
        fig, ((ax1, ax3),
            (ax2, ax4),
            (ax5, ax6)) = plt.subplots(nrows=3, ncols=2, figsize=figsize, dpi=dpi)
    else:
        fig, (ax1, ax2, ax5) = plt.subplots(nrows=3, ncols=1, figsize=figsize, dpi=dpi)

    ax1.set_title('%s Board IQ' % first_label)
    ax1.plot(I, label='I')
    ax1.plot(Q, label='Q')
    ax2.set_title('%s Board Power' % first_label)
    ax2.plot(power, label='Power', color='red')
    
    # Add threshold lines
    ax2.axhline(y=rising_threshold, color='green', linestyle='--', alpha=0.7, label=f'Rising Threshold ({rising_threshold:.0f})')
    ax2.axhline(y=falling_threshold, color='orange', linestyle='--', alpha=0.7, label=f'Falling Threshold ({falling_threshold:.0f})')

positive_edge, negative_edge = edge_detector(power, rising_threshold, falling_threshold, cycles_to_debounce)
pos_edge_indexes = np.argwhere(positive_edge).flatten()

if args.gui or args.save_png:
    for i in pos_edge_indexes:
        ax2.plot(i, rising_threshold, 'g^', markersize=6)

time_delta_pos_edges = np.diff(pos_edge_indexes)
avg = np.average(time_delta_pos_edges)
var = np.var(time_delta_pos_edges)
dev = np.std(time_delta_pos_edges)
if print_stats:
    print("\nTimestamp Count between Rising Edges:")
    print(f"  Average:  {avg:.2f}")
    print(f"  Variance: {var:.2f}")
    print(f"  Std.Dev:  {dev:.2f}")
    print(f"  Edge Count: {len(pos_edge_indexes)}")

neg_edge_indexes = np.argwhere(negative_edge).flatten()

if args.gui or args.save_png:
    for i in neg_edge_indexes:
        ax2.plot(i, falling_threshold, 'yv', markersize=6)

time_delta_neg_edges = np.diff(neg_edge_indexes)
avg = np.average(time_delta_neg_edges)
var = np.var(time_delta_neg_edges)
dev = np.std(time_delta_neg_edges)
if print_stats:
    print("\nTimestamp Count between Falling Edges:")
    print(f"  Average: {avg:.2f}")
    print(f"  Variance:{var:.2f}")
    print(f"  Std.Dev: {dev:.2f}")
    print(f"  Edge Count: {len(neg_edge_indexes)}")

print("\n%s Board" % first_label)
print(f"  Measured Timestamp: {(avg):.3f} samples, Error: {(avg-period):.3f} samples")

try:
    fill_expected = int(burst*fill/100)
    fill = neg_edge_indexes - pos_edge_indexes
    avg = np.average(fill)
    var = np.var(fill)
    dev = np.std(fill)
    fill_vs_burst = 100 * avg/burst
except ValueError as err:
    print(f"[Error] %s Board: Edge count imbalanced.\n{err}" % first_label)
    print(err)
    fill = None
    fill_vs_burst = -1


if args.gui or args.save_png:
    ax5.set_title("Fill Error")
    ax5.set_xlabel("Error (samples)")
    fill_error = fill - fill_expected
    ax5.hist(fill_error, alpha=0.7, bins=30, color=('skyblue'))

error_samples=(avg-fill_expected)

jout[first_label] = {
    "measured_fill_percent": fill_vs_burst,
    "error_samples": error_samples}

print(f"  Measured Fill:      {fill_vs_burst:.2f}%, Error: {error_samples:.2f} samples")

if args.enforce:
    local_pass = abs(error_samples) <= args.enforce
    test_pass = False if not local_pass else test_pass
    print(f"  Test threshold:      {args.enforce}, Result: %s" % ("PASS" if local_pass else "FAIL"))


################################################################
# TX Loopback Compare
################################################################
if plot_count == 2:
    data = pd.read_csv('compare.csv')
    I = data['I'].to_numpy()
    Q = data['Q'].to_numpy()
    power = I**2 + Q**2
    num_samples = range(len(I))

    if args.gui or args.save_png:
        ax3.set_title('TX Loopback Compare IQ')
        ax3.plot(I, label='I')
        ax3.plot(Q, label='Q')
        ax4.set_title('TX Loopback Compare Power')
        ax4.plot(power, label='Power', color='red')
        
        # Add threshold lines
        ax4.axhline(y=rising_threshold, color='green', linestyle='--', alpha=0.7, label=f'Rising Threshold ({rising_threshold:.0f})')
        ax4.axhline(y=falling_threshold, color='orange', linestyle='--', alpha=0.7, label=f'Falling Threshold ({falling_threshold:.0f})')

    positive_edge, negative_edge = edge_detector(power, rising_threshold, falling_threshold, cycles_to_debounce)
    pos_edge_indexes = np.argwhere(positive_edge).flatten()

    if args.gui or args.save_png:
        for i in pos_edge_indexes:
            ax4.plot(i, rising_threshold, 'g^', markersize=6)

    time_delta_pos_edges = np.diff(pos_edge_indexes)
    avg = np.average(time_delta_pos_edges)
    var = np.var(time_delta_pos_edges)
    dev = np.std(time_delta_pos_edges)
    if print_stats:
        print("\nTimestamp Count between Rising Edges:")
        print(f"  Average:  {avg:.2f}")
        print(f"  Variance: {var:.2f}")
        print(f"  Std.Dev:  {dev:.2f}")
        print(f"  Edge Count: {len(pos_edge_indexes)}")

    neg_edge_indexes = np.argwhere(negative_edge).flatten()

    if args.gui or args.save_png:
        for i in neg_edge_indexes:
            ax4.plot(i, falling_threshold, 'yv', markersize=6)

    time_delta_neg_edges = np.diff(neg_edge_indexes)
    avg = np.average(time_delta_neg_edges)
    var = np.var(time_delta_neg_edges)
    dev = np.std(time_delta_neg_edges)
    if print_stats:
        print("\nTimestamp Count between Falling Edges:")
        print(f"  Average: {avg:.2f}")
        print(f"  Variance:{var:.2f}")
        print(f"  Std.Dev: {dev:.2f}")
        print(f"  Edge Count: {len(neg_edge_indexes)}")


    jout["TX"] = {"measured_timestamp": avg}
    print("\nTX Board: loopback compare")
    print(f"  Measured Timestamp: {(avg):.3f} samples")

    try:
        fill_compare = neg_edge_indexes - pos_edge_indexes
        avg = np.average(fill_compare)
        var = np.var(fill_compare)
        dev = np.std(fill_compare)
        fill_vs_burst_compare = 100 * avg/burst
    except ValueError as err:
        print(f"[Error] TX Compare: Edge count imbalanced.\n{err}")
        print(err)
        fill_compare = None
        fill_vs_burst_compare = -1

    error_samples=(avg-fill_expected)
    jout["TX"]["measured_fill_percent"] = fill_vs_burst_compare
    jout["TX"]["error_samples"] = error_samples
    print(f"  Measured Fill:      {fill_vs_burst_compare:.2f}%, Error: {error_samples:.2f} samples")

    if args.enforce:
        local_pass = error_samples <= args.enforce
        test_pass = False if not local_pass else test_pass
        print(f"  Test threshold:      {args.enforce}, Result: %s" % ("PASS" if local_pass else "FAIL"))

    print("")
    print(f"Fill Delta:")
    fill_delta_avg = 100*(np.average(fill_compare) - np.average(fill))/burst
    jout["TX"]["fill_delta_average"] = fill_delta_avg
    print(f"  Average:  {fill_delta_avg:.2f}%")

    if args.gui or args.save_png:
        ax6.set_title("Fill Error")
        ax6.set_xlabel("Error (samples)")
        fill_error = fill_compare - fill_expected
        ax6.hist(fill_error, alpha=0.7, bins=30, color=('skyblue'))

# File Ops
if args.enforce:
    jout["exit"] = 1 if not test_pass else 0
if args.output:
    with open(args.output, 'w') as json_file:
        json.dump(jout, json_file, indent=4)

# early exit, as the rest of the script is GUI stuff
if not args.gui and not args.save_png:
    exit(1 if not test_pass else 0)

# General plot assignments
if plot_count == 2:
    axis = (ax1, ax2, ax3, ax4)
    axis_iq = (ax1, ax3)
    axis_power = (ax2, ax4)
else:
    axis = (ax1, ax2)
    axis_iq = (ax1,)
    axis_power = (ax2,)

for ax in axis:
    ax.legend(loc='upper right')
    ax.set_ylabel('Power')
    ax.set_xlabel('Sample Index')
for ax in axis_power:
    ax.set_ylim(bottom=-1e6, top=9e6)
for ax in axis_iq:
    ax.set_ylim(bottom=-2500, top=2500)

fig.subplots_adjust(hspace=1)

if args.save_png:
    plt.savefig(args.save_png, dpi=dpi)
    print(f"Plot saved to {args.save_png} ({png_width}x{png_height} pixels)")
elif args.gui:
    print(f"\nPress [Escape] to close figure")
    fig.canvas.mpl_connect('key_press_event', close_figure)
    plt.show()

exit(1 if not test_pass else 0)
