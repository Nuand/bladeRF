%-------------------------------------------------------------------------
% This file is part of the bladeRF project
%
% Copyright (C) 2016 Nuand LLC
%
% This program is free software; you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation; either version 2 of the License, or
% (at your option) any later version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License along
% with this program; if not, write to the Free Software Foundation, Inc.,
% 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
%-------------------------------------------------------------------------

function [bits, info] = fsk_receive(preamble_waveform, iq_signal, ...
                                    decimation_factor, samps_per_symb, ...
                                    h, num_bytes)
% FSK_RECEIVE Demodulate/receive data bits from an FSK baseband IQ signal.
% Correlates the iq_signal with the given preamble waveform to find the
% start of the data signal
%    [BITS, INFO] = fsk_receive(PREAMBLE_WAVEFORM, IQ_SIGNAL,
%                              SAMPS_PER_SYMB, NUM_BYTES);
%
%    PREAMBLE_WAVEFORM preamble iq samples waveform to determine start of
%    data in the the iq_signal
%
%    IQ_SIGNAL baseband CPFSK signal to receive
%
%    DECIMATION_FACTOR amount to decimate by when performing correlation
%    with preamble. 1 = no decimation. 2 = use every other sample.
%
%    SAMPS_PER_SYMB number of samples per symbol
%
%    H modulation index used on the transmitted signal
%
%    NUM_BYTES number of data bytes to demodulate from the iq_signal
%
%    BITS is an Nx8 matrix of received bits. Each bit element in
%    the matrix is a char which can either be '1' or '0'. Each row of the
%    matrix is a byte - containing 8 of these bit elements. Each byte is
%    received in order from smallest row index to largest row index. Each
%    bit in the byte is received in order from largest column index to
%    smallest column index (i.e. LSb first, MSb last))
%          Example: if bits is   | '0' '0' '0' '0' '1' '1' '1' '1'|
%                                | '0' '0' '1' '1' '1' '0' '1' '0'|
%                   then the received bit stream was:
%                                     "11110000 01011100"
%    Note: You can use dec2bin('test string', 8) to convert a string to
%    this bit matrix format, and bin2dec(bits) to convert a bit matrix to a
%    string
%
%    INFO a struct containing information to use for debug plots
%        INFO.iq_filt_norm  = filtered + normalized IQ signal
%        INFO.preamble_corr = cross correlation of .iq_filt_norm and
%                                preamble waveform
%        INFO.dphase        = vector of phase changes in the .iq_filt_norm

info = struct(    'iq_filt_norm', -1, ...
                'preamble_corr', -1, ...
                'dphase', -1);

corr_peak_thresh = 0.5625 * (length(preamble_waveform)/decimation_factor)^2;

%Low pass filter the IQ signal
passband = 1/samps_per_symb * h*2/pi;
stopband = 1/3 * 8/samps_per_symb * h*2/pi;
if (stopband < 1)
	%Design filter
    b = remez(31, [0 passband stopband 1], [1 1 0 0]);
    iq_signal = filter(b, 1, iq_signal);
end

%Normalize signal to [-1, 1]
iq_signal = iq_signal / max([ max(real(iq_signal)) max(imag(iq_signal)) ]);
info.iq_filt_norm = iq_signal;

%Correlate input IQ signal with known preamble waveform
%Decimate iq signal and preamble waveform
iq_signal_dec = iq_signal(1:decimation_factor:end);
preamble_waveform_dec = preamble_waveform(1:decimation_factor:end);
corr = xcorr(iq_signal_dec, preamble_waveform_dec);
info.preamble_corr = corr;

%Find peaks in cross correlation power (i^2 + q^2)
[peaks, peak_locs] = findpeaks(abs(corr).^2);
%Attempt to find a peak which passes the threshold
peak_index = -1;
for i = 1:length(peaks)
    if (peaks(i) >= corr_peak_thresh)
        peak_index = peak_locs(i);
        break;
    end
end
%Stop if no preamble match
if (peak_index == -1)
    fprintf(2, 'fsk_receive(): No preamble match in signal\n');
    bits = -1;
    return;
end
%Calculate iq_signal start index based on this peak
%Number of zeros that were padded to the end of preamble waveform inside
%the xcorr() function so that both vectors were the same length:
num_padded_zeros = length(iq_signal_dec) - length(preamble_waveform_dec);
sig_start_index = peak_index - num_padded_zeros;
%Calculate the un-decimated iq_signal start index
sig_start_index = sig_start_index * decimation_factor - (decimation_factor-1);

%Demodulate bits from the IQ signal at the start index
[bits, info.dphase] = fsk_demod(iq_signal(sig_start_index:end), samps_per_symb, num_bytes);

end
