function [ signal, signal_i, signal_q ] = load_sc16q11(filename)
% LOAD_SC16Q11 Read a normalized complex signal from a binary file in the 
%   bladeRF "SC16 Q11" format.
%
%   [SIGNAL, SIGNAL_I, SIGNAL_Q] = load_sc16q11(FILENAME)
%
%   FILENAME is the source filename.
% 
%   SIGNAL is a complex signal with the real and imaginary components
%   within the range [-1.0, 1.0).
%
%   SIGNAL_I and SIGNAL_Q are optional return values which contain the 
%   real and imaginary components of SIGNAL as separate vectors.
%
    [f, err_msg] = fopen(filename, 'r', 'ieee-le');
    if f ~= -1
        data = fread(f, Inf, 'int16');
        
        signal_i = data(1:2:end - 1, :) ./ 2048.0;
        signal_q = data(2:2:end,     :) ./ 2048.0;
        
        signal = signal_i + 1j .* signal_q;
        
        fclose(f);
    else
        error(err_msg);
    end
end
