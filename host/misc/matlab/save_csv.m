function save_csv(filename, signal)
% SAVE_SC16Q11 Write a normalized complex signal to a CSV file in the
%              as integer bladeRF "SC16 Q11" values.
%
%   FILENAME is the target filename. The file will be overwritten if it
%   already exists.
%
%   SIGNAL is a complex signal with the real and imaginary components
%   within the range [-1.0, 1.0).

    sig_i = round(real(signal) .* 2048.0);
    sig_i(sig_i > 2047)  = 2047;
    sig_i(sig_i < -2048) = -2048;

    sig_q = round(imag(signal) .* 2048.0);
    sig_q(sig_q > 2048)  = 2047;
    sig_q(sig_q < -2048) = -2048;

    sig = [sig_i sig_q];
    csvwrite(filename, sig);
end

