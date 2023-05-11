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
import argparse
import subprocess

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
fill = 50
burst = 50e3
period = 100e3
iterations = 10
threshold = 1500
devarg_tx = ""
devarg_rx = ""
print_stats = False

parser = argparse.ArgumentParser(
    description='TXRX hardware loop timestamp validation',
    allow_abbrev=False,
    add_help=True
)
parser.add_argument('-f', '--fill', type=float, help='fill (%%)')
parser.add_argument('-b', '--burst', type=int, help='burst length (in samples)')
parser.add_argument('-p', '--period', type=int, help='period length (in samples)')
parser.add_argument('-i', '--iterations', type=int, help='number of pulses')
parser.add_argument('-t', '--threshold', type=int, help='edge count amplitude threshold')
parser.add_argument('-tx', '--txdev', type=str, help='TX device string')
parser.add_argument('-rx', '--rxdev', type=str, help='RX device string')
parser.add_argument('-c', '--compare', help='RF loopback compare', action="store_true", default=False)
parser.add_argument('-s', '--stats', help='print edge statistics', action="store_true", default=False)

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
if args.rxdev:
    dev_rx = args.rxdev
    devarg_rx = f"-r {dev_rx}"
if args.stats:
    print_stats = True

cycles_to_debounce = 0.25 * burst * fill/100

################################################################
# Generate Pulse
################################################################
binary_file = 'libbladeRF_test_txrx_hwloop'
output_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(output_dir)
proc = subprocess.run(f"./{binary_file} --fill {fill} --burst {burst} --period {period} "
                      f"--iterations {iterations} -l {devarg_tx} {devarg_rx} {args.compare * '-c'}", shell=True)

if proc.returncode != 0:
    print("Failed to run hwloop binary")
    exit(1)

################################################################
# RX Data Analysis
################################################################
data = pd.read_csv('samples.csv')
I = data['I'].to_numpy()
Q = data['Q'].to_numpy()
amp = np.abs(I + Q*1j)
num_samples = range(len(I))

fig, ((ax1, ax3), (ax2, ax4)) = plt.subplots(nrows=2, ncols=2, figsize=(16, 6))
ax1.plot(I, label='I')
ax1.plot(Q, label='Q')
ax1.set_title('RX Board IQ')
ax1.set_xlabel('Sample Index')
ax1.set_ylabel('Amplitude')
ax1.legend(loc='upper right')
ax2.plot(amp, label='Amplitude', color='red')
ax2.set_title('RX Board Magnitude')
ax2.set_xlabel('Sample Index')
ax2.set_ylabel('Amplitude')
ax2.legend(loc='upper right')

positive_edge, negative_edge = edge_detector(amp, threshold, threshold, cycles_to_debounce)
pos_edge_indexes = np.argwhere(positive_edge).flatten()
for i in pos_edge_indexes:
    ax2.plot(i, threshold, 'g_', markersize=10)

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
for i in neg_edge_indexes:
    ax2.plot(i, threshold, 'y_', markersize=10)

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

print("\nRX Board")
print(f"  Predicted Timestamp: {(avg):.3f} samples")

try:
    fill = neg_edge_indexes - pos_edge_indexes
    avg = np.average(fill)
    var = np.var(fill)
    dev = np.std(fill)
    fill_vs_burst = 100 * avg/burst
except ValueError as err:
    print(f"[Error] RX Board: Edge count imbalanced.\n{err}")
    print(err)
    fill = None

print(f"  Predicted Fill:      {fill_vs_burst:.2f}%")

################################################################
# TX Loopback Compare
################################################################
data = pd.read_csv('compare.csv')
I = data['I'].to_numpy()
Q = data['Q'].to_numpy()
amp = np.abs(I + Q*1j)
num_samples = range(len(I))

ax3.plot(I, label='I')
ax3.plot(Q, label='Q')
ax3.set_title('TX Loopback Compare IQ')
ax3.set_xlabel('Sample Index')
ax3.set_ylabel('Amplitude')
ax3.legend(loc='upper right')
ax4.plot(amp, label='Amplitude', color='red')
ax4.set_title('TX Loopback Compare Magnitude')
ax4.set_xlabel('Sample Index')
ax4.set_ylabel('Amplitude')
ax4.legend(loc='upper right')

positive_edge, negative_edge = edge_detector(amp, threshold, threshold, cycles_to_debounce)
pos_edge_indexes = np.argwhere(positive_edge).flatten()
for i in pos_edge_indexes:
    ax4.plot(i, threshold, 'g_', markersize=10)

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
for i in neg_edge_indexes:
    ax4.plot(i, threshold, 'y_', markersize=10)

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

    print("\nTX Board: loopback compare")
    print(f"  Predicted Timestamp: {(avg):.3f} samples")

try:
    fill = neg_edge_indexes - pos_edge_indexes
    avg = np.average(fill)
    var = np.var(fill)
    dev = np.std(fill)
    compare_fill_vs_burst = 100 * avg/burst
except ValueError as err:
    print(f"[Error] TX Compare: Edge count imbalanced.\n{err}")
    print(err)
    fill = None

print(f"  Predicted Fill:      {compare_fill_vs_burst:.2f}%")

fig.subplots_adjust(hspace=0.5)
print(f"\nPress [Escape] to close figure")
fig.canvas.mpl_connect('key_press_event', close_figure)
plt.show()
