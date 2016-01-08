% This class implements a bladeRF MATLAB System Simulink block.
%
% The bladeRF_Simulink block interfaces with a single bladeRF device and is
% capable of utilizing both the transmit and receive paths on the device. The
% block properties may be used to enable and use RX, TX, or both.
%
% To use this block, place a "MATLAB System" block in your Simulink Model and
% specify "bladeRF_Simulink" for the system object name.
%
% Next, configure the device by double clicking on the block. Here, a few
% groups of settings are presented:
%  * Device             Device selection and device-wide settings
%  * RX Configuration   RX-specific settings
%  * TX Configuration   TX-specific settings
%  * Miscellaneous      Other library-specific options
%
% Currently only "Interpreted Execution" is supported. Be sure to select
% this in the first tab.
%
% In the "Device" tab, the "Device Specification String" allows one to specify
% which device to use if multiple bladeRFs are connected.  For example, to use
% a device with a serial number starting with a3f..., a valid device
% specification string would be:
%
%           "*:serial=a3f"
%
% Alternatively, one can specify the "Nth" device if the block<->device
% assignments do not matter. For two devices, one could use:
%           "*:instance=0" and "*:instance=1"
%
% If left blank, this string will select the first available device.
%
% To enable the receive output port, check the "Enable Receiver" checkbox in
% the "RX Configuration" tab.  Similarly to enable the transmit input port,
% check the "Enable Transmitter" checkbox in the "TX Configuration" tab.
%
% See also: bladeRF

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

classdef bladeRF_Simulink < matlab.System & ...
                            matlab.system.mixin.Propagates & ...
                            matlab.system.mixin.CustomIcon
    %% Properties
    properties
        verbosity           = 'Info'    % libbladeRF verbosity

        rx_frequency        = 915e6;    % Frequency [230e6, 3.8e9]
        rx_lna              = 6         % LNA Gain  [0, 3, 6]
        rx_vga1             = 30;       % VGA1 Gain [5, 30]
        rx_vga2             = 0;        % VGA2 Gain [0, 30]

        tx_frequency        = 920e6;    % Frequency [230e6, 3.8e9]
        tx_vga1             = -8;       % VGA1 Gain [-35, -4]
        tx_vga2             = 16;       % VGA2 Gain [0, 25]
    end

    properties(Nontunable)
        device_string       = '';       % Device specification string
        loopback_mode       = 'None'    % Active loopback mode

        rx_bandwidth        = '1.5';    % LPF Bandwidth (MHz)
        rx_samplerate       = 3e6;      % Sample rate
        rx_num_buffers      = 64;       % Number of stream buffers to use
        rx_num_transfers    = 16;       % Number of USB transfers to use
        rx_buf_size         = 16384;    % Size of each stream buffer, in samples (must be multiple of 1024)
        rx_step_size        = 16384;    % Number of samples to RX during each simulation step
        rx_timeout_ms       = 5000;     % Stream timeout (ms)

        tx_bandwidth        = '1.5';    % LPF Bandwidth (MHz)
        tx_samplerate       = 3e6;      % Sample rate
        tx_num_buffers      = 64;       % Number of stream buffers to use
        tx_num_transfers    = 16;       % Number of USB transfers to use
        tx_buf_size         = 16384;    % Size of each stream buffer, in samples (must be multiple of 1024)
        tx_step_size        = 16384;    % Number of samples to TX during each simulation step
        tx_timeout_ms       = 5000;     % Stream timeout (ms)
    end

    properties(Logical, Nontunable)
        enable_rx           = true;     % Enable Receiver
        enable_overrun      = false;    % Enable Overrun output
        enable_tx           = false;    % Enable Transmitter
        enable_underrun     = false;    % Enable Underrun output (for future use)
        xb200               = false     % Enable use of XB-200 (must be attached)
    end

    properties(Hidden, Transient)
        rx_bandwidthSet = matlab.system.StringSet({ ...
            '1.5',  '1.75', '2.5',  '2.75',  ...
            '3',    '3.84', '5',    '5.5',   ...
            '6',    '7',    '8.75', '10',    ...
            '12',   '14',   '20',   '28'     ...
        });

        tx_bandwidthSet = matlab.system.StringSet({ ...
            '1.5',  '1.75', '2.5',  '2.75',  ...
            '3',    '3.84', '5',    '5.5',   ...
            '6',    '7',    '8.75', '10',    ...
            '12',   '14',   '20',   '28'     ...
        });

        loopback_modeSet = matlab.system.StringSet({
            'None', ...
            'BB_TXLPF_RXVGA2', 'BB_TXVGA1_RXVGA2', 'BB_TXLPF_RXLPF', ...
            'RF_LNA1', 'RF_LNA2', 'RF_LNA3', ...
            'Firmware'
        });

        verbositySet = matlab.system.StringSet({
            'Verbose', 'Debug', 'Info', 'Warning', 'Critical', 'Silent' ...
        });
    end

    properties (Access = private)
        device = []

        % Cache previously set tunable values to avoid querying the device
        % for all properties when only one changes.
        curr_rx_frequency
        curr_rx_lna
        curr_rx_vga1
        curr_rx_vga2
        curr_tx_frequency
        curr_tx_vga1
        curr_tx_vga2
    end

    %% Static Methods
    methods (Static, Access = protected)
        function groups = getPropertyGroupsImpl
            device_section_group = matlab.system.display.SectionGroup(...
                'Title', 'Device', ...
                'PropertyList', {'device_string', 'loopback_mode', 'xb200' } ...
            );

            rx_gain_section = matlab.system.display.Section(...
                'Title', 'Gain', ...
                'PropertyList', { 'rx_lna', 'rx_vga1', 'rx_vga2'} ...
            );

            rx_stream_section = matlab.system.display.Section(...
                'Title', 'Stream', ...
                'PropertyList', {'rx_num_buffers', 'rx_num_transfers', 'rx_buf_size', 'rx_timeout_ms', 'rx_step_size', } ...
            );

            rx_section_group = matlab.system.display.SectionGroup(...
                'Title', 'RX Configuration', ...
                'PropertyList', { 'enable_rx', 'enable_overrun', 'rx_frequency', 'rx_samplerate', 'rx_bandwidth' }, ...
                'Sections', [ rx_gain_section, rx_stream_section ] ...
            );

            tx_gain_section = matlab.system.display.Section(...
                'Title', 'Gain', ...
                'PropertyList', { 'tx_vga1', 'tx_vga2'} ...
            );

            tx_stream_section = matlab.system.display.Section(...
                'Title', 'Stream', ...
                'PropertyList', {'tx_num_buffers', 'tx_num_transfers', 'tx_buf_size', 'tx_timeout_ms', 'tx_step_size', } ...
            );

            tx_section_group = matlab.system.display.SectionGroup(...
                'Title', 'TX Configuration', ...
                'PropertyList', { 'enable_tx', 'enable_underrun', 'tx_frequency', 'tx_samplerate', 'tx_bandwidth' }, ...
                'Sections', [ tx_gain_section, tx_stream_section ] ...
            );

            misc_section_group = matlab.system.display.SectionGroup(...
                'Title', 'Miscellaneous', ...
                'PropertyList', {'verbosity'} ...
            );

            groups = [ device_section_group, rx_section_group, tx_section_group, misc_section_group ];
        end

        function header = getHeaderImpl
            text = 'This block provides access to a Nuand bladeRF device via libbladeRF MATLAB bindings.';
            header = matlab.system.display.Header('bladeRF_Simulink', ...
                'Title', 'bladeRF', 'Text',  text ...
            );
        end
    end

    methods (Access = protected)
        %% Output setup
        function count = getNumOutputsImpl(obj)
            if obj.enable_rx == true
                if obj.enable_overrun == true
                    count = 2;
                else
                    count = 1;
                end
            else
                count = 0;
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                count = count + 1;
            end
        end

        function varargout = getOutputNamesImpl(obj)
            if obj.enable_rx == true
                varargout{1} = 'RX Samples';
                n = 2;

                if obj.enable_overrun == true
                    varargout{n} = 'RX Overrun';
                    n = n + 1;
                end
            else
                n = 1;
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                varargout{n} = 'TX Underrun';
            end
        end

        function varargout = getOutputDataTypeImpl(obj)
            if obj.enable_rx == true
                varargout{1} = 'double';    % RX Samples
                n = 2;

                if obj.enable_overrun == true
                    varargout{n} = 'logical';   % RX Overrun
                    n = n + 1;
                end
            else
                n = 1;
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                varargout{n} = 'logical';   % TX Underrun
            end
        end

        function varargout = getOutputSizeImpl(obj)
            if obj.enable_rx == true
                varargout{1} = [obj.rx_step_size 1];  % RX Samples
                n = 2;

                if obj.enable_overrun == true
                    varargout{n} = [1 1]; % RX Overrun
                    n = n + 1;
                end
            else
                n = 1;
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                varargout{n} = [1 1]; % TX Underrun
            end
        end

        function varargout = isOutputComplexImpl(obj)
            if obj.enable_rx == true
                varargout{1} = true;    % RX Samples
                n = 2;
            else
                n = 1;
            end

            if obj.enable_overrun == true
                varargout{n} = false;   % RX Overrun
                n = n + 1;
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                varargout{n} = false;   % TX Underrun
            end
        end

        function varargout  = isOutputFixedSizeImpl(obj)
            if obj.enable_rx == true
                varargout{1} = true;    % RX Samples
                varargout{2} = true;    % RX Overrun
                n = 3;
            else
                n = 1;
            end

            if obj.enable_overrun == true
            end

            if obj.enable_tx == true && obj.enable_underrun == true
                varargout{n} = true;    % TX Underrun
            end
        end

        %% Input setup
        function count = getNumInputsImpl(obj)
            if obj.enable_tx == true
                count = 1;
            else
                count = 0;
            end
        end

        function varargout = getInputNamesImpl(obj)
            if obj.enable_tx == true
                varargout{1} = 'TX Samples';
            else
                varargout = {};
            end
        end

        %% Property and Execution Handlers
        function icon = getIconImpl(~)
            icon = sprintf('Nuand\nbladeRF');
        end

        function setupImpl(obj)
            %% Library setup
            bladeRF.log_level(obj.verbosity);

            %% Device setup
            if obj.xb200 == true
                xb = 'XB200';
            else
                xb = [];
            end

            obj.device = bladeRF(obj.device_string, [], xb);
            obj.device.loopback = obj.loopback_mode;

            %% RX Setup
            obj.device.rx.config.num_buffers   = obj.rx_num_buffers;
            obj.device.rx.config.buffer_size   = obj.rx_buf_size;
            obj.device.rx.config.num_transfers = obj.rx_num_transfers;
            obj.device.rx.config.timeout_ms    = obj.rx_timeout_ms;

            obj.device.rx.frequency  = obj.rx_frequency;
            obj.curr_rx_frequency    = obj.device.rx.frequency;

            obj.device.rx.samplerate = obj.rx_samplerate;
            obj.device.rx.bandwidth  = str2double(obj.rx_bandwidth) * 1e6;

            obj.device.rx.lna        = obj.rx_lna;
            obj.curr_rx_lna          = bladeRF.str2lna(obj.device.rx.lna);

            obj.device.rx.vga1       = obj.rx_vga1;
            obj.curr_rx_vga1         = obj.device.rx.vga1;

            obj.device.rx.vga2       = obj.rx_vga2;
            obj.curr_rx_vga2         = obj.device.rx.vga2;

            %% TX Setup
            obj.device.tx.config.num_buffers   = obj.tx_num_buffers;
            obj.device.tx.config.buffer_size   = obj.tx_buf_size;
            obj.device.tx.config.num_transfers = obj.tx_num_transfers;
            obj.device.tx.config.timeout_ms    = obj.tx_timeout_ms;

            obj.device.tx.samplerate = obj.tx_samplerate;
            obj.device.tx.bandwidth  = str2double(obj.tx_bandwidth) * 1e6;

            obj.device.tx.frequency  = obj.tx_frequency;
            obj.curr_tx_frequency    = obj.device.tx.frequency;

            obj.device.tx.vga1       = obj.tx_vga1;
            obj.curr_tx_vga1         = obj.device.tx.vga1;

            obj.device.tx.vga2       = obj.tx_vga2;
            obj.curr_tx_vga2         = obj.device.tx.vga2;

        end

        function releaseImpl(obj)
            delete(obj.device);
        end

        function resetImpl(obj)
            obj.device.rx.stop();
            obj.device.tx.stop();
        end

        % Perform a read of received samples and an 'overrun' array that denotes whether
        % the associated samples is invalid due to a detected overrun.
        function varargout = stepImpl(obj, varargin)
            varargout = {};

            if obj.enable_rx == true
                if obj.device.rx.running == false
                    obj.device.rx.start();
                end

                [rx_samples, ~, ~, rx_overrun] = obj.device.receive(obj.rx_step_size);

                varargout{1} = rx_samples;
                varargout{2} = rx_overrun;
                out_idx = 3;
            else
                out_idx = 1;
            end

            if obj.enable_tx == true
                if obj.device.tx.running == false
                    obj.device.tx.start();
                end

                obj.device.transmit(varargin{1});

                % Detecting TX Underrun is not yet supported by libbladeRF.
                % This is for future use.
                varargout{out_idx} = false;
            end
        end

        function processTunedPropertiesImpl(obj)

            %% RX Properties
            if isChangedProperty(obj, 'rx_frequency') && obj.rx_frequency ~= obj.curr_rx_frequency
                obj.device.rx.frequency = obj.rx_frequency;
                obj.curr_rx_frequency   = obj.device.rx.frequency;
                %disp('Updated RX frequency');
            end

            if isChangedProperty(obj, 'rx_lna') && obj.rx_lna ~= obj.curr_rx_lna
                obj.device.rx.lna = obj.rx_lna;
                obj.curr_rx_lna   = bladeRF.str2lna(obj.device.rx.lna);
                %disp('Updated RX LNA gain');
            end

            if isChangedProperty(obj, 'rx_vga1') && obj.rx_vga1 ~= obj.curr_rx_vga1
                obj.device.rx.vga1 = obj.rx_vga1;
                obj.curr_rx_vga1   = obj.device.rx.vga1;
                %disp('Updated RX VGA1 gain');
            end

            if isChangedProperty(obj, 'rx_vga2') && obj.rx_vga2 ~= obj.curr_rx_vga2
                obj.device.rx.vga2 = obj.rx_vga2;
                obj.curr_rx_vga2   = obj.device.rx.vga2;
                %disp('Updated RX VGA2 gain');
            end

            %% TX Properties
            if isChangedProperty(obj, 'tx_frequency') && obj.tx_frequency ~= obj.curr_tx_frequency
                obj.device.tx.frequency = obj.tx_frequency;
                obj.curr_tx_frequency   = obj.device.rx.frequency;
                %disp('Updated TX frequency');
            end

            if isChangedProperty(obj, 'tx_vga1') && obj.tx_vga1 ~= obj.curr_tx_vga1
                obj.device.tx.vga1 = obj.tx_vga1;
                obj.curr_tx_vga1   = obj.device.tx.vga1;
                %disp('Updated TX VGA1 gain');
            end

            if isChangedProperty(obj, 'tx_vga2') && obj.tx_vga2 ~= obj.curr_tx_vga2
                obj.device.tx.vga2 = obj.tx_vga2;
                obj.curr_tx_vga2   = obj.device.tx.vga2;
                %disp('Updated TX VGA2 gain');
            end

        end

        function validatePropertiesImpl(obj)
            if obj.enable_rx == false && obj.enable_tx == false
                warning('Neither bladeRF RX or TX is enabled. One or both should be enabled.');
            end

            if isempty(obj.device)
                tx_min_freq = 0;
                rx_min_freq = 0;
            else
                tx_min_freq = obj.device.tx.min_frequency;
                rx_min_freq = obj.device.rx.min_frequency;
            end

            %% Validate RX properties
            if obj.rx_num_buffers < 1
                error('rx_num_buffers must be > 0.');
            end

            if obj.rx_num_transfers >= obj.rx_num_buffers
                error('rx_num_transfers must be < rx_num_buffers.');
            end

            if obj.rx_buf_size < 1024 || mod(obj.rx_buf_size, 1024) ~= 0
                error('rx_buf_size must be a multiple of 1024.');
            end

            if obj.rx_timeout_ms < 0
                error('rx_timeout_ms must be >= 0.');
            end

            if obj.rx_step_size <= 0
                error('rx_step_size must be > 0.');
            end

            if obj.rx_samplerate < 160.0e3
                error('rx_samplerate must be >= 160 kHz.');
            elseif obj.rx_samplerate > 40e6
                error('rx_samplerate must be <= 40 MHz.')
            end

            if obj.rx_frequency < rx_min_freq
                error(['rx_frequency must be >= ' num2str(rx_min_freq) '.']);
            elseif obj.rx_frequency > 3.8e9
                error('rx_frequency must be <= 3.8 GHz.');
            end

            if obj.rx_lna ~= 0 && obj.rx_lna ~= 3 && obj.rx_lna ~= 6
                error('rx_lna must be one of the following: 0, 3, 6');
            end

            if obj.rx_vga1 < 5
                error('rx_vga1 gain must be >= 5.')
            elseif obj.rx_vga1 > 30
                error('rx_vga1 gain must be <= 30.');
            end

            if obj.rx_vga2 < 0
                error('rx_vga2 gain must be >= 0.');
            elseif obj.rx_vga2 > 30
                error('rx_vga2 gain must be <= 30.');
            end

            %% Validate TX Properties
            if obj.tx_num_buffers < 1
                error('tx_num_buffers must be > 0.');
            end

            if obj.tx_num_transfers >= obj.tx_num_buffers
                error('tx_num_transfers must be < tx_num_transfers');
            end

            if obj.tx_buf_size < 1024 || mod(obj.tx_buf_size, 1024) ~= 0
                error('tx_buf_size must be a multiple of 1024.');
            end

            if obj.tx_timeout_ms < 0
                error('tx_timeout_ms must be >= 0.');
            end

            if obj.tx_step_size <= 0
                error('tx_step_size must be > 0.');
            end

            if obj.tx_samplerate < 160.0e3
                error('tx_samplerate must be >= 160 kHz.');
            elseif obj.tx_samplerate > 40e6
                error('tx_samplerate must be <= 40 MHz.')
            end

            if obj.tx_frequency < tx_min_freq;
                error(['tx_frequency must be >= ' num2str(tx_min_freq) '.']);
            elseif obj.tx_frequency > 3.8e9
                error('tx_frequency must be <= 3.8 GHz.');
            end

            if obj.tx_vga1 < -35
                error('tx_vga1 gain must be >= -35.')
            elseif obj.tx_vga1 > -4
                error('tx_vga1 gain must be <= -4.');
            end

            if obj.tx_vga2 < 0
                error('tx_vga2 gain must be >= 0.');
            elseif obj.tx_vga2 > 25
                error('tx_vga2 gain must be <= 25.');
            end
        end
    end
end
