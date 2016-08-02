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

function [iq_samples] = fsk_transmit(training_seq, preamble, data, ...
                                    samps_per_symb, mod_index)
% FSK_TRANSMIT Produce a baseband FSK signal of the input binary data, with
% a ramp up/ramp down at the beginning/end of the signal.
%    [IQ_SAMPLES] = fsk_transmit(TRAINING_SEQ, PREAMBLE, DATA,
%                                SAMPS_PER_SYMB, MOD_INDEX);
%
%    TRAINING_SEQ is a matrix of training sequence bits to transmit. Format
%    specified below.
%
%    PREAMBLE is a matrix of preamble bits to transmit. Format specified
%    below
%
%    DATA is a matrix of data bits to transmit. Format specified below.
%
%    Bits matrix format: Each bit element in the matrix is a char which can
%    either be '1' or '0'. Each row of the matrix is a byte - containing 8
%    of these bit elements. Each byte is transmitted in order from smallest
%    row index to largest row index. Each bit in the byte is transmitted in
%    order from largest column index to smallest column index
%    (i.e. LSb first, MSb last))
%     Example: if the matrix is  | '0' '0' '0' '0' '1' '1' '1' '1'|
%                                | '0' '0' '1' '1' '1' '0' '1' '0'|
%               then the transmitted bit stream will be:
%                                    "11110000 01011100"
%    Note: You can use dec2bin('test string', 8) to convert a string to
%    this bit matrix format, and bin2dec(bits) to convert a bit matrix to a
%    string
%
%    SAMPS_PER_SYMB number of samples per symbol
%
%    MOD_INDEX phase modulation index: the phase deviation per symbol
%
%    IQ_SAMPLES is the resulting complex (IQ) FSK modulated baseband
%    signal with real and imaginary components within the range [-1.0, 1.0].
%    The following shows the content and ordering of this signal:
%    / ramp up | training_seq | preamble | data | ramp down \
%    The ramps at the beginning/end of the signal have a length of
%    'samps_per_symb'

%----Assemble bitstream to transmit
%| training sequence | preamble | data |
bits = training_seq;
bits(size(bits, 1)+1 : size(bits,1)+size(preamble, 1), :) = preamble;
bits(size(bits, 1)+1 : size(bits,1)+size(data, 1), :) = data;

%----Construct IQ samples vector
%-Ramp up the I samples from 0 to 1, and leave Q samples as zeros
%Length of the ramp is 'samps_per_symb' samples
iq_samples(1:samps_per_symb) = 1/samps_per_symb : 1/samps_per_symb: 1;

%Modulate the bit stream, and add to iq_samples
sig = fsk_mod(bits, samps_per_symb, mod_index, angle(iq_samples(end)));
iq_samples(length(iq_samples)+1:length(iq_samples)+length(sig)) = sig;

%-Ramp down the samples (either I or Q) to 0.
%Length of the ramp is 'samps_per_symb' samples
last_samp = iq_samples(end);
if (real(last_samp) ~= 0)
    rampdown_I = real(last_samp): -real(last_samp)/samps_per_symb: 0;
else
    rampdown_I(1:samps_per_symb+1) = 0;
end
if (imag(last_samp) ~= 0)
    rampdown_Q = imag(last_samp): -imag(last_samp)/samps_per_symb: 0;
else
    rampdown_Q(1:samps_per_symb+1) = 0;
end
iq_samples(length(iq_samples)+1 : length(iq_samples)+samps_per_symb) = ...
    rampdown_I(2:samps_per_symb+1) + 1j*rampdown_Q(2:samps_per_symb+1);

end
