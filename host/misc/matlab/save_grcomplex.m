function [ret] = save_grcomplex(filename, signal)
% SAVE_GRCOMPLEX Write a normalized complex signal to a binary file in the
%                GNU Radio complex float format.
%
%   [RET] = save_grcomplex(FILENAME, SIGNAL)
%
%   RET is 1 on success, and 0 on failure.
%
%   FILENAME is the target filename. The file will be overwritten if it
%   already exists. The file data is written in little-endian format.
%
%   SIGNAL is a complex signal with the real and imaginary components
%   within the range [-1.0, 1.0].

    [f, err_msg] = fopen(filename, 'w', 'ieee-le');
    if f ~= -1

        sig_i = real(signal);
        sig_q = imag(signal);

        sig_len = 2 * length(sig_i);

        sig_out(1:2:sig_len-1) = sig_i;
        sig_out(2:2:sig_len)   = sig_q;

        count = fwrite(f, sig_out, 'float');
        fclose(f);

        if count == sig_len;
            ret = 1;
        else
            ret = 0;
        end
    else
        error(err_msg);
        ret = 0;
    end
end
