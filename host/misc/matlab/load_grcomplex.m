function [ signal, signal_i, signal_q ] = load_grcomplex(filename)
% LOAD_GRCOMPLEX Read a complex signal from a binary file containing
%                complex float samples saved using GNU Radio.
%
%                The data in the file is assumed to be little-endian.
%
%
%   [SIGNAL, SIGNAL_I, SIGNAL_Q] = load_grcomplex(FILENAME)
%
%   FILENAME is the source filename.
%
%   SIGNAL is a complex signal with the real and imaginary components
%   within the range [-1.0, 1.0].
%
%   SIGNAL_I and SIGNAL_Q are optional return values which contain the
%   real and imaginary components of SIGNAL as separate vectors.
%
    [f, err_msg] = fopen(filename, 'r', 'ieee-le');
    if f ~= -1
        data = fread(f, Inf, 'float');

        signal_i = data(1:2:end-1, :);
        signal_q = data(2:2:end,   :);

        signal = signal_i + 1j .* signal_q;

        fclose(f);
    else
        error(err_msg)
    end
end
