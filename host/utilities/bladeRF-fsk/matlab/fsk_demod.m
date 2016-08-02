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

function [bits, dphase] = fsk_demod(fsk_signal, samps_per_symb, num_bytes)
% FSK_DEMOD Produce demodulated bit stream from baseband CPFSK signal
%    Demodulated bit stream contains (nrows*ncols) bits. A '1' corresponds
%    to a positive frequency (increasing phase) while a '0' corresponds to a
%    negative frequency (decreasing phase).
%    [FSK_SIGNAL] = fsk_demod(FSK_SIGNAL, SAMPS_PER_SYMB, NROWS, NCOLS)
%
%    FSK_SIGNAL is the resulting complex (IQ) FSK modulated baseband signal
%    with real and imaginary components within the range [-1.0, 1.0].
%
%    SAMPS_PER_SYMB number of samples per symbol
%
%    NUM_BYTES number of bytes to demodulate from the signal
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
%                                      "11110000 01011100"
%    Note: You can use dec2bin('test string', 8) to convert a string to
%    this bit matrix format, and bin2dec(bits) to convert a bit matrix to a
%    string

%Check to make sure the iq signal is long enough to demod 'num_bytes' bytes
if (length(fsk_signal) < samps_per_symb*num_bytes*8)
    fprintf(2, ['fsk_demod(): Signal is not long enough to demod the', ...
                ' desired number of bytes (%d)\n'], num_bytes);
    bits = -1;
    dphase = -1;
    return;
end

%Sum up all changes in phase during symbol period
%If total change is positive -> a 1 was sent
%If total change is negative -> a 0 was sent

currPos = 2;    %First sample just defines initial phase
%Loop through each expected byte
for byte = 1:num_bytes
    %Loop through 8 bits
    for bit = 8:-1:1
        %Unwrap 1+samps_per_symb angles corresponding to these
        %samps_per_symb samples AND the sample preceding them
        phase = unwrap(angle(fsk_signal(currPos-1:currPos+samps_per_symb-1)));
        dphase_tot = 0;                %Initialize dphase_tot to 0
        for samp = 2:samps_per_symb+1
            dphase_tot = dphase_tot + (phase(samp) - phase(samp-1));
            %Update dphase array
            dphase(currPos - 1) = phase(samp) - phase(samp-1);
            currPos = currPos + 1;
        end
        if dphase_tot > 0
            bits(byte, bit) = '1';
        else
            bits(byte, bit) = '0';
        end
    end
end

end
