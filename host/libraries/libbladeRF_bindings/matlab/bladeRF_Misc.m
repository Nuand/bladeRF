% bladeRF miscellaneous control and configuration.
%
% This is a submodule of the bladeRF object. It is not intended to be
% accessed directly, but through the top-level bladeRF object.
%


%
% Copyright (c) 2018 Nuand LLC
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

classdef bladeRF_Misc < handle

    properties(SetAccess=immutable, Hidden=true)
        bladerf
    end

    properties(SetAccess=immutable)
        dc_power        % Read current power consumption
        dc_source       % Read DC power source
        temperature     % Read current temperature in 'C
    end

    properties(Dependent = true)
    end

    methods

        % Read the current device temperature
        function val = get.temperature(obj)
            val = single(0);
            tmp = single(0);
            [status, ~, val] = calllib('libbladeRF', 'bladerf_get_rfic_temperature', obj.bladerf.device, tmp);
            bladeRF.check_status('bladerf_get_rfic_temperature', status);
        end

        % Read the current DC power consumption
        function val = get.dc_power(obj)
            val = single(0);
            tmp = single(0);
            [status, ~, val] = calllib('libbladeRF', 'bladerf_get_pmic_register', obj.bladerf.device, 'BLADERF_PMIC_POWER', tmp);
            bladeRF.check_status('bladerf_get_pmic_register', status);
        end

        % Read the DC power supply
        function val = get.dc_source(obj)
            source = int32(0);
            tmp = int32(0);
            [status, ~, source] = calllib('libbladeRF', 'bladerf_get_power_source', obj.bladerf.device, tmp);
            bladeRF.check_status('bladerf_get_power_source', status);
            switch source
                case 'BLADERF_UNKNOWN'
                    val = 'UNKNOWN';
                case 'BLADERF_PS_DC'
                    val = 'DC';
                case 'BLADERF_PS_USB_VBUS'
                    val = 'USB';
                otherwise
                    val = 'UNKNOWN'
            end
        end

        % Construtor
        function obj = bladeRF_Misc(dev)
            obj.bladerf = dev;
        end
    end
end
