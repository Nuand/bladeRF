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

function [fsk_signal] = fsk_mod(bits, samps_per_symb, mod_index, initial_phase)
% FSK_MOD Produce baseband FSK signal of the input bit stream. A '1'
% corresponds to a positive frequency (increasing phase), while a '0'
% corresponds to a negative frequency (decreasing phase).
%    [FSK_SIGNAL] = fsk_mod(BITS, SAMPS_PER_SYMB, MOD_INDEX, INITIAL_PHASE)
%
%    BITS is an Nx8 matrix of bits to transmit. Each bit element in
%    the matrix is a char which can either be '1' or '0'. Each row of the
%    matrix is a byte - containing 8 of these bit elements. Each byte is
%    transmitted in order from smallest row index to largest row index. Each
%    bit in the byte is transmitted in order from largest column index to
%    smallest column index (i.e. LSb first, MSb last))
%          Example: if bits is   | '0' '0' '0' '0' '1' '1' '1' '1'|
%                                | '0' '0' '1' '1' '1' '0' '1' '0'|
%                   then the transmitted bit stream will be:
%                                         "11110000 01011100"
%    Note: You can use dec2bin('test string', 8) to convert a string to
%    this bit matrix format, and bin2dec(bits) to convert a bit matrix to a
%    string
%
%    SAMPS_PER_SYMB number of samples per symbol
%
%    MOD_INDEX phase modulation index: the phase deviation per symbol
%
%    FSK_SIGNAL is the resulting complex (IQ) CPFSK modulated baseband
%    signal with real and imaginary components within the range [-1.0, 1.0].
%
%    INITIAL_PHASE Initial phase to use as a reference (every sample is a
%    change in phase, so an initial phase needs to be defined)

%Calculate phase change per sample
dphase_abs = mod_index / samps_per_symb;

phase = initial_phase;        %Set initial phase
i = 1;                        %Current samples position
%Loop through each byte
for byte = 1:size(bits,1)
    %loop through each bit in the byte in reverse index order
    for bit = 8:-1:1
        if bits(byte, bit) == '1'    %Transmit 1
            dphase = +dphase_abs;
        elseif bits(byte, bit) == '0'
            dphase = -dphase_abs;
        else
            fprintf(2, 'Unidentified character ''%c''. Skipping.\n', ...
                        bits(byte, bit));
            continue;
        end
        %Loop through each of the 'samps_per_symb' samples in the symbol
        for samp = 1:samps_per_symb
            phase_prev = phase;
            phase = mod(phase_prev + dphase, 2*pi);
            fsk_signal(i) = exp(1j * phase);
            i = i + 1;
        end
    end
end

end
