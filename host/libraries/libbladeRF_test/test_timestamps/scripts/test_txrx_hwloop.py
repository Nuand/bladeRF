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

################################################################
# Generate Pulse
################################################################
binary_file = 'libbladeRF_test_txrx_hwloop'
output_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(output_dir)
proc = subprocess.run(f"./{binary_file} --fill {fill} --burst {burst} --period {period} "
                      f"--iterations {iterations} -l {devarg_tx} {devarg_rx}", shell=True)

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

fig, (ax1, ax2) = plt.subplots(nrows=2, ncols=1, figsize=(8, 6))
ax1.plot(I, label='I')
ax1.plot(Q, label='Q')
ax1.set_title('I and Q Components')
ax1.set_xlabel('Sample Index')
ax1.set_ylabel('Amplitude')
ax1.legend(loc='upper right')
ax2.plot(amp, label='Amplitude', color='red')
ax2.set_title('Amplitude')
ax2.set_xlabel('Sample Index')
ax2.set_ylabel('Amplitude')
ax2.legend(loc='upper right')

positive_edge = np.diff(np.sign(amp - threshold)) > 0
pos_edge_indexes = np.argwhere(positive_edge).flatten()
for i in pos_edge_indexes:
    ax2.plot(i, threshold, 'g_', markersize=10)

time_delta_pos_edges = np.diff(pos_edge_indexes)
mode, mode_count = st.mode(time_delta_pos_edges)
avg = np.average(time_delta_pos_edges)
var = np.var(time_delta_pos_edges)
dev = np.std(time_delta_pos_edges)
print("\nTimestamp Count between Rising Edges:")
print(f"  Average:  {avg:.2f}")
print(f"  Variance: {var:.2f}")
print(f"  Std.Dev:  {avg:.2f}")
print(f"  Edge Count: {len(pos_edge_indexes)}")

negative_edge = np.diff(np.sign(amp - threshold)) < 0
neg_edge_indexes = np.argwhere(negative_edge).flatten()
for i in neg_edge_indexes:
    ax2.plot(i, threshold, 'y_', markersize=10)

time_delta_neg_edges = np.diff(neg_edge_indexes)
avg = np.average(time_delta_neg_edges)
var = np.var(time_delta_neg_edges)
dev = np.std(time_delta_neg_edges)
print("\nTimestamp Count between Falling Edges:")
print(f"  Average: {avg:.2f}")
print(f"  Variance:{var:.2f}")
print(f"  Std.Dev: {avg:.2f}")
print(f"  Edge Count: {len(neg_edge_indexes)}")
print(f"\nPredicted Timestamp: {(mode[0]/1e3):.2f}k samples @ {mode_count[0]}/{iterations-1} periods")

fill = neg_edge_indexes - pos_edge_indexes
avg = np.average(fill)
var = np.var(fill)
dev = np.std(fill)
mode, mode_count = st.mode(fill)
fill_vs_burst = mode[0]*100/burst
print(f"Predicted Fill:      {fill_vs_burst:.2f}% @ {mode_count[0]}/{iterations} cycles")

fig.subplots_adjust(hspace=0.5)
print(f"\nPress [Escape] to close figure")
fig.canvas.mpl_connect('key_press_event', close_figure)
plt.show()
