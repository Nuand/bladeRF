% bladeRF IQ corrections for DC offset and IQ imbalance
%
% This is a submodule of the bladeRF object. It is not intended to be accessed
% directly, but through a top-level handle to a bladeRF object.
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

%% IQ Corrections for DC offset and gain/phase imbalance
classdef bladeRF_IQCorr < handle
    properties(Access = private)
        bladerf
        module
    end

    properties(Dependent = true)
        dc_i    % I channel DC offset compensation. Valid range is [-2048, 2048].
        dc_q    % Q channel DC offset compensation. Valid range is [-2048, 2048].
        gain    % IQ imbalance gain compensation. Valid range is [-1.0, 1.0].
        phase   % IQ imbalance phase compensation. Valid range is [-10.0, 10.0] degrees.
    end

    methods
        % Constructor
        function obj = bladeRF_IQCorr(dev, module, dc_i, dc_q, phase, gain)
            obj.bladerf = dev;
            obj.module  = module;
            obj.dc_i    = dc_i;
            obj.dc_q    = dc_q;
            obj.phase   = phase;
            obj.gain    = gain;
        end

        % Set I channel DC offset correction value
        function set.dc_i(obj, val)
            if val < -2048 || val > 2048
                error('DC offset correction value for I channel must be within [-2048, 2048].');
            end

            [status, ~] = calllib('libbladeRF', 'bladerf_set_correction', ...
                                  obj.bladerf.device, ...
                                  obj.module, ...
                                  'BLADERF_CORR_LMS_DCOFF_I', ...
                                  int16(val));

            bladeRF.check_status('bladerf_set_correction:dc_i', status);

            %fprintf('Set %s I DC offset correction: %d\n', obj.module, val);
        end

        % Set Q channel DC offset correction value
        function obj = set.dc_q(obj, val)
            if val < -2048 || val > 2048
                error('DC offset correction value for Q channel must be within [-2048, 2048].');
            end

            [status, ~] = calllib('libbladeRF', 'bladerf_set_correction', ...
                                  obj.bladerf.device, ...
                                  obj.module, ...
                                  'BLADERF_CORR_LMS_DCOFF_Q', ...
                                  int16(val));

            bladeRF.check_status('bladerf_set_correction:dc_q', status);

            %fprintf('Set %s Q DC offset correction: %d\n', obj.module, val);
        end

        % Read current I channel DC offset correction value
        function val = get.dc_i(obj)
            val = int16(0);
            [status, ~, val] = calllib('libbladeRF', 'bladerf_get_correction', ...
                                       obj.bladerf.device, ...
                                       obj.module, ...
                                       'BLADERF_CORR_LMS_DCOFF_I', ...
                                       val);

            bladeRF.check_status('bladerf_get_correction:dc_i', status);

            %fprintf('Read %s I DC offset: %d\n', obj.module, val);
        end

        % Read current Q channel DC offset correction value
        function val = get.dc_q(obj)
            val = int16(0);
            [status, ~, val] = calllib('libbladeRF', 'bladerf_get_correction', ...
                                       obj.bladerf.device, ...
                                       obj.module, ...
                                       'BLADERF_CORR_LMS_DCOFF_Q', ...
                                       val);

            bladeRF.check_status('bladerf_get_correction:dc_q', status);

            %fprintf('Read %s Q DC offset: %d\n', obj.module, val);
        end

        % Set phase correction value
        function obj = set.phase(obj, val_deg)
            if val_deg < -10 || val_deg > 10
                error('Phase correction value must be within [-10, 10] degrees.');
            end

            val_counts = round((val_deg * 4096 / 10));

            [status, ~] = calllib('libbladeRF', 'bladerf_set_correction', ...
                                  obj.bladerf.device, ...
                                  obj.module, ...
                                  'BLADERF_CORR_FPGA_PHASE', ...
                                  val_counts);

            bladeRF.check_status('bladerf_set_correction:phase', status);

            %fprintf('Set %s phase correction: %f (%d)\n', obj.module, val_deg, val_counts);
        end

        % Read current phase correction value
        function val_deg = get.phase(obj)
            val_counts = int16(0);
            [status, ~, x] = calllib('libbladeRF', 'bladerf_get_correction', ...
                                     obj.bladerf.device, ...
                                     obj.module, ...
                                     'BLADERF_CORR_FPGA_PHASE', ...
                                     val_counts);

            bladeRF.check_status('bladerf_get_correction:phase', status);

            val_deg = double(x) / (4096 / 10);

            %fprintf('Read %s phase correction: %f (%d)\n', obj.module, val_deg, val_counts);
        end

        % Set an IQ gain correction value
        function obj = set.gain(obj, val)
            if val < -1.0 || val > 1.0
                error('Gain correction value must be within [-1.0, 1.0]');
            end

            val = val * 4096;

            [status, ~] = calllib('libbladeRF', 'bladerf_set_correction', ...
                                  obj.bladerf.device, ...
                                  obj.module, ...
                                  'BLADERF_CORR_FPGA_GAIN', ...
                                  int16(val));

            bladeRF.check_status('bladerf_set_correction:gain', status);

            %fprintf('Set %s IQ gain correction value: %d\n', obj.module, val);
        end

        % Read the current IQ gain correction value
        function val_dbl = get.gain(obj)
            val = int16(0);
            [status, ~, val] = calllib('libbladeRF', 'bladerf_get_correction', ...
                                       obj.bladerf.device, ...
                                       obj.module, ...
                                       'BLADERF_CORR_FPGA_GAIN', ...
                                       val);

            bladeRF.check_status('bladerf_get_correction:gain', status);

            val_dbl = double(val) / 4096.0;

            %fprintf('Read %s IQ gain correction value: %d\n', obj.module, val_dbl);
        end
    end
end
