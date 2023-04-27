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

def close_figure(event):
    if event.key == 'escape':
        plt.close()

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
ax1.legend()
ax2.plot(amp, label='Amplitude', color='red')
ax2.set_title('Amplitude')
ax2.set_xlabel('Sample Index')
ax2.set_ylabel('Amplitude')
ax2.legend()

threshold = 1800
positive_edge = np.diff(np.sign(amp - threshold)) > 0
edge_indexes = np.argwhere(positive_edge).flatten()
for i in edge_indexes:
    ax2.plot(i, threshold, 'g_', markersize=10)

time_between_edges = np.diff(edge_indexes)
avg = np.average(time_between_edges)
var = np.var(time_between_edges)
dev = np.std(time_between_edges)

print("\nRising Edge Results:")
print(f"  Average:  {avg:.2f}")
print(f"  Variance: {var:.2f}")
print(f"  Std.Dev:  {avg:.2f}")

fig.subplots_adjust(hspace=0.5)
print(f"\nPress [Escape] to close figure")
fig.canvas.mpl_connect('key_press_event', close_figure)
plt.show()
