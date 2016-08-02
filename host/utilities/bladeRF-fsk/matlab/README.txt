This set of MATLAB files will generate/receive binary CPFSK baseband waveforms.
Works on both MATLAB and GNU Octave.

Run fsk.m to simulate the FSK modem just in MATLAB.
Run fsk_csv.m to write/read IQ samples to/from a csv file so they can be
    transmitted/received on an RF carrier with a bladeRF. In order to run this
    successfully you need to add bladeRF/host/misc/matlab to your matlab path
    (for the save_csv() and load_csv() functions)

fsk_mod(): FSK baseband modulator function
- generates CPFSK baseband IQ waveform of the given set of bits

fsk_demod(): FSK baseband demodulator function
- extracts the bit sequence of the given CPFSK baseband waveform

fsk_transmit(): Generates baseband signal for an FSK frame
- Calls fsk_mod() with FSK frame, which includes training sequence and preamble
  in addition to user data
- Adds a ramp up/ramp down to the beginning/end of the signal

fsk_receive(): Receives an FSK frame from a baseband signal
- Low-pass filters and normalizes the input signal
- Correlates the input signal with the given preamble waveform to determine
  start of FSK data
- Calls fsk_demod() to extract bits from the FSK data signal

fsk.m: Script for simulating mod/demod just in MATLAB
- prompts for an input string (spaces are welcome)
- converts ASCII string to a set of bits
- calls fsk_transmit to generate the CPFSK baseband IQ waveform
- adds attenuation and noise to the signal to simulate a channel
- calls fsk_receive to process the waveform and extract data bits
- converts bits to ASCII string
- prints received string to command window

fsk_csv.m: Script for using the modulator/demodulator with samples csv files
- prompts for an input string (spaces are welcome)
- converts ASCII string to a set of bits
- calls fsk_transmit to generate the CPFSK baseband IQ waveform
- writes the samples to a tx csv file
- waits for the user to transmit the samples and receive IQ samples
  in a different csv file (e.g. rx_samples.csv)
- reads in samples from the rx csv file
- calls fsk_receive to process the waveform and extract data bits
- converts bits to ASCII string
- prints received string to command window
