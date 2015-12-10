function [ret] = save_sc16q11(filename, signal)
% SAVE_SC16Q11 Write a normalized complex signal to a binary file in the
%   bladeRF "SC16 Q11" format.
%
%   [RET] = save_sc16q11(FILENAME, SIGNAL)
%
%   RET is 1 on success, and 0 on failure.
%
%   FILENAME is the target filename. The file will be overwritten if it
%   already exists. The file data is written in little-endian format.
%
%   SIGNAL is a complex signal with the real and imaginary components
%   within the range [-1.0, 1.0).

    [f, err_msg] = fopen(filename, 'w', 'ieee-le');
    if f ~= -1

        sig_i = round(real(signal) .* 2048.0);
        sig_i(sig_i > 2047)  = 2047;
        sig_i(sig_i < -2048) = -2048;

        sig_q = round(imag(signal) .* 2048.0);
        sig_q(sig_q > 2047)  = 2047;
        sig_q(sig_q < -2048) = -2048;

        assert(length(sig_i) == length(sig_q));
        sig_len = 2 * length(sig_i);

        sig_out(1:2:sig_len - 1) = sig_i;
        sig_out(2:2:sig_len)     = sig_q;

        count = fwrite(f, sig_out, 'int16');
        fclose(f);

        if count == sig_len;
            ret = 1;
        else
        end
    else
        error(err_msg);
        ret = 0;
    end
end
