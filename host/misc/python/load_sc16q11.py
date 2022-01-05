import math
import numpy as np


SC16Q11_DTYPE = np.int16


def load_sc16q11(file_name: str, n_channels: int = 1, iq_dtype=np.complex128) -> np.ndarray:
    """
    Load SC16Q11 binary file into numpy array

    Args:
        file_name: file name to a SC16Q11 binary file
        n_channels: number of channels
        iq_dtype: dtype for the output IQ samples

    Returns:
        numpy.ndarray of shape (n_channels, n_iq_samples_per_channel) where n_iq_samples_per_channel
        is the number of complex iq samples corresponding to each channel

    """
    with open(file_name, 'rb') as f:
        samples = np.fromfile(f, dtype=SC16Q11_DTYPE)
        n_iq_samples_per_channel = math.floor(len(samples) / n_channels / 2)
        iq = np.empty((n_channels, n_iq_samples_per_channel), dtype=iq_dtype)

        for channel in range(n_channels):
            iq[channel, :] = (samples[channel + 0::n_channels * 2] + 1j * samples[channel + 1::n_channels * 2]) / 2048.

    return iq
