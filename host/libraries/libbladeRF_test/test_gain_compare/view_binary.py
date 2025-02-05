import numpy as np
import matplotlib.pyplot as plt
import argparse
import os


def read_samples(file_name, num_channels):
    """Read samples from a file and split them into channels."""
    int12_max = 2047
    samples = np.fromfile(file_name, dtype=np.int16)
    samples = samples[int(40e3):] # Flush out garbage samples
    if num_channels > 1:
        samples_ch1 = samples[::4] + 1j*samples[1::4]
        samples_ch2 = samples[2::4] + 1j*samples[3::4]
    else:
        samples_ch1 = samples[::2] + 1j*samples[1::2]

    return samples_ch1, samples_ch2 if num_channels > 1 else None

def calculate_fft_dbfs(samples, adc_max_value=2047):
    """Calculate the FFT and convert it to dBFS for given samples."""

    fft_result = np.fft.fftshift(np.fft.fft(samples, n=4096))
    normalized_magnitude = np.abs(fft_result) / len(np.abs(fft_result))
    # Convert to dBFS
    magnitude_dbfs = 20 * np.log10(normalized_magnitude / adc_max_value)
    avg_dbfs = np.mean(magnitude_dbfs[magnitude_dbfs != -np.inf])  # Avoid log(0) which results in -inf

    return magnitude_dbfs, avg_dbfs

def plot_data(samples_ch1, samples_ch2=None, alpha=0.95):
    """Plot the sample data in decibel scale."""

    # Time domain plot in linear scale (if you want it in dB, convert to dB)
    plt.figure()
    plt.plot(
        np.abs(samples_ch1),
        label=f"CH1: avg = {np.abs(samples_ch1).mean():.2f}",
        alpha=alpha,
    )

    if samples_ch2 is not None:
        plt.plot(
            np.abs(samples_ch2),
            label=f"CH2: avg = {np.abs(samples_ch2).mean():.2f}",
            alpha=alpha,
        )

    plt.ylim(0, np.abs(2047 + 1j * 2047))
    plt.title('Time-domain Signal')
    plt.xlabel('Sample Number')
    plt.ylabel('Amplitude')
    plt.legend(loc="upper right")

    # FFT computation and dBFS conversion
    plt.figure()
    CH1_mag_dbfs, CH1_avg_dbfs = calculate_fft_dbfs(samples_ch1)

    plt.plot(
        CH1_mag_dbfs,
        label=f"CH1",
        alpha=alpha,
    )

    plt.axhline(
        y=CH1_avg_dbfs,
        color="g",
        linestyle="--",
        label=f"CH1 Avg: {CH1_avg_dbfs:.2f} dBFS",
    )

    if samples_ch2 is not None:
        CH2_mag_dbfs, CH2_avg_dbfs = calculate_fft_dbfs(samples_ch2)

        plt.plot(
            CH2_mag_dbfs,
            label=f"CH2",
            alpha=alpha,
        )

        plt.axhline(
            y=CH2_avg_dbfs,
            color="r",
            linestyle="--",
            label=f"CH2 Avg: {CH2_avg_dbfs:.2f} dBFS",
        )

    plt.title('Frequency-domain Signal (FFT in dBFS)')
    plt.xlabel('Frequency Bin')
    plt.ylabel('Magnitude (dBFS)')
    plt.legend(loc="upper right")
    plt.show()

def main():
    parser = argparse.ArgumentParser(description="Sample Configuration")

    parser.add_argument("-f", "--file", type=str, default="dual_test.bin", help="File name")
    parser.add_argument(
        "-n",
        "--num_channels",
        type=int,
        default=1,
        help="Number of channels (default: 1)",
    )
    args = parser.parse_args()

    if args.num_channels <= 0:
        raise ValueError("Number of channels must be a positive integer.")

    print("Configuration:")
    print(f"File Name: {args.file}")
    print(f"Number of Channels: {args.num_channels}")

    try:
        samples_ch1, samples_ch2 = read_samples(args.file, args.num_channels)
        plot_data(samples_ch1, samples_ch2)
    except FileNotFoundError:
        print(f"Error: File '{args.file}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")


if __name__ == "__main__":
    main()
