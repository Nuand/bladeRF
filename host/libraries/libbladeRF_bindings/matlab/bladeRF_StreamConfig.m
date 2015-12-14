% bladeRF stream configuration parameters.
%
% This is a submodule of the bladeRF object. It is not intended to be
% accessed directly, but through the top-level bladeRF object.
%

%
% Copyright (c) 2015 Nuand LLC
%
% Permission is hereby granted, free of charge, to any person obtaining a copy
% of this software and associated documentation files (the "Software"), to deal
% in the Software without restriction, including without limitation the rights
% to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
% copies of the Software, and to permit persons to whom the Software is
% furnished to do so, subject to the following conditions:
%
% The above copyright notice and this permission notice shall be included in
% all copies or substantial portions of the Software.
%
% THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
% IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
% FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
% AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
% LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
% OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
% THE SOFTWARE.
%

% Configuration of synchronous RX/TX sample stream
classdef bladeRF_StreamConfig < handle
    properties(Access=private)
        locked
    end

    properties
        num_buffers         % Total number of buffers to use in the stream.
        buffer_size         % Size of buffer, in samples. Mus be a multiple of 1024.
        num_transfers       % Number of USB transfers to utilize. Must be < num_buffers.
        timeout_ms          % Stream timeout, in milliseconds.
    end

    methods
        function lock(obj)
            obj.locked = true;
        end

        function unlock(obj)
            obj.locked = false;
        end

        function set.num_buffers(obj, val)
            if val < 1
                error('The stream buffer count must be positive.');
            end

            if obj.locked == false
                obj.num_buffers = val;
            else
                error('Cannot modify num_buffers while streaming.');
            end
        end

        function set.buffer_size(obj, val)
            if val < 1024 || mod(val, 1024) ~= 0
                error('Stream buffer sizes must be a multiple of 1024.');
            end

            if obj.locked == false
                obj.buffer_size = val;
            else
                error('Cannot modify buffer_size while streaming.');
            end
        end

        function set.num_transfers(obj, val)
            if val >= obj.num_buffers
                error('The number of transfers used in a stream must be < number of buffers.');
            end

            if obj.locked == false
                obj.num_transfers = val;
            else
                error('Cannot modify num_transfers while streaming.');
            end
        end

        function set.timeout_ms(obj, val)
            if val < 0
                error('Stream timeout must be >= 0');
            end

            if obj.locked == false
                obj.timeout_ms = val;
            else
                error('Cannot modify timeout_ms while streaming');
            end
        end

        function obj = bladeRF_StreamConfig(num_buffers, buffer_size, num_transfers, timeout_ms)
            obj.locked          = false;

            if nargin < 1
                obj.num_buffers     = 64;
            else
                obj.num_buffers     = num_buffers;
            end

            if nargin < 2
                obj.buffer_size     = 16384;
            else
                obj.buffer_size     = buffer_size;
            end

            if nargin < 3
                obj.num_transfers   = 16;
            else
                obj.num_transfers   = num_transfers;
            end

            if nargin < 4
                obj.timeout_ms      = 5000;
            else
                obj.timeout_ms      = timeout_ms;
            end
        end
    end
end
