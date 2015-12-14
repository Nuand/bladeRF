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

% IQ Corrections for DC offset and gain/phase imbalance
classdef bladeRF_IQCorr < handle
    properties(Access = private)
        bladerf
        module

        rx_tuned_low  % Flag used when performing TX DC cal. Indicates F_RX < F_TX.
    end

    properties(Dependent = true)
        dc_i    % I channel DC offset compensation. Valid range is [-2048, 2048].
        dc_q    % Q channel DC offset compensation. Valid range is [-2048, 2048].
        gain    % IQ imbalance gain compensation. Valid range is [-1.0, 1.0].
        phase   % IQ imbalance phase compensation. Valid range is [-10.0, 10.0] degrees.
    end

    properties(Constant, Access = private)

        rx_dc_cal_samplerate = 3e6;
        rx_dc_cal_bw         = 1.5e6;

        % Filter used to isolate contribution of TX LO leakage in received
        % signal. 15th order Equiripple FIR with Fs=4e6, Fpass=1, Fstop=1e6
        tx_cal_filt = [ ...
            0.000327949366768 0.002460188536582 0.009842382390924 ...
            0.027274728394777 0.057835200476419 0.098632713294830 ...
            0.139062540460741 0.164562494987592 0.164562494987592 ...
            0.139062540460741 0.098632713294830 0.057835200476419 ...
            0.027274728394777 0.009842382390924 0.002460188536582 ...
            0.000327949366768
        ];

        tx_dc_cal_samplerate = 4e6;
        tx_dc_cal_tx_bw      = 1.5e6;
        tx_dc_cal_rx_bw      = 3e6;

        tx_dc_cal_lna        = 'MAX';
        tx_dc_cal_rxvga1     = 25;
        tx_dc_cal_rxvga2     = 0;

        tx_dc_cal_rx_cfg     = bladeRF_StreamConfig(64, 16384, 16, 5000);
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
        function set.dc_q(obj, val)
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
        function set.phase(obj, val_deg)
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
        function set.gain(obj, val)
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

        function results = auto_dc(obj, freq_list, print_status)
        % Compute DC correction values for the specified frequencies.
        %
        % bladeRF/calibrate('ALL') should be run prior to this function.
        %
        % results = bladeRF_IQCorr/auto_dc(frequencies, print status);
        %
        % frequencies  - One or more frequencies to calculate DC correction values for.
        %                If not specified, the current frequency is used.
        %
        % print_status - If set to `true`, this will print a status
        %                messages. Defaults to `false`.
        %
        % results      - This is a matrix that has one row per frequency
        %                in freq_list. The elements in columns are:
        %                [ i_corr q_corr i_err q_err ]
        %
        %                Where i_corr and q_corr are the computed
        %                correction values, and i_err and q_err are
        %                estimated error values.
        %
        % Examples
        %
        % For a single frequency:
        %
        % >> device.rx.corrections.auto_dc(920e6)
        % ans =
        %   316.0000  284.0000    0.0003    0.0008
        %
        %
        % For multiple frequencies:
        %
        % >> rxcal(b, [433e6 921e6 2.415e9], true)
        % Running RX DC calibration for F=433000000
        % Running RX DC calibration for F=921000000
        % Running RX DC calibration for F=2415000000
        %
        % ans =
        %   263.0000   39.0000    0.0007    0.0005
        %   311.0000  247.0000    0.0003    0.0001
        %   278.0000  534.0000    0.0009    0.0006
        %
            if obj.bladerf.rx.running == true || obj.bladerf.tx.running == true
                error('This operation requires that the device not be actively streaming.');
            end

            if nargin < 3
                print_status = false;
            end

            if strcmp(obj.module, 'BLADERF_MODULE_TX') == true
                if nargin < 2 || isempty(freq_list)
                    freq_list = [obj.bladerf.tx.frequency];
                end

                if nargin < 4
                    external_loopback = false;
                end

                results = obj.tx_auto_dc(freq_list, print_status, external_loopback);
            else
                if nargin < 2 || isempty(freq_list)
                    freq_list = [obj.bladerf.rx.frequency];
                end

                results = obj.rx_auto_dc(freq_list, print_status);
            end
        end
    end


    methods (Static, Access = private)
        function [sig] = fs4_mix(sig, shift_left)
            if nargin < 2
                % "Shift left" in the freq domain. Mix with -Fs/4.
                shift_left = true;
            end

            if shift_left == true
                m_inc = 1;
            else
                m_inc = -1;
            end

            m = 0;
            for n = 1:length(sig);
                switch m
                    case 0
                        sig(n) =  real(sig(n)) + 1j *  imag(sig(n));

                    case 1
                        sig(n) =  imag(sig(n)) + 1j * -real(sig(n));

                    case 2
                        sig(n) = -real(sig(n)) + 1j * -imag(sig(n));

                    case 3
                        sig(n) = -imag(sig(n)) + 1j *  real(sig(n));
                end

                m = mod(m + m_inc, 4);
            end
        end
    end

    methods (Access = private)
        function [samples, ts] = rx_samples(obj, ts_in, count, n_inc)
            retry = 0;
            max_retries = 10;
            overrun = true;
            dev = obj.bladerf;
            ts = ts_in;

            while overrun == true && retry < max_retries
                [samples, ~, ~, overrun] = dev.receive(count, 1000, ts);

                if overrun
                    fprintf('Got an overrun. Retrying...');
                    ts = ts + count + 5 * n_inc;
                    retry = retry + 1;
                end
            end

            if retry >= max_retries
                error('Overrun limit occurred while receving samples.')
            end

            ts = ts + count + n_inc;
        end

        function results = rx_auto_dc(obj, freq_list, print_status)
            dev = obj.bladerf;

            backup.samplerate = dev.rx.samplerate;
            backup.bandwidth  = dev.rx.bandwidth;
            backup.config     = dev.rx.config;
            backup.tx_freq    = dev.tx.frequency;

            results = zeros(length(freq_list), 4);

            if nargin < 3
                print_status = false;
            end

            dev.rx.samplerate = obj.rx_dc_cal_samplerate;
            dev.rx.bandwidth  = obj.rx_dc_cal_bw;

            dev.rx.config.num_buffers   = 64;
            dev.rx.config.buffer_size   = 16384;
            dev.rx.config.num_transfers = 16;
            dev.rx.config.timeout_ms    = 1000;

            % Number of samples between each reception
            t_inc = 0.015 * dev.rx.samplerate;

            % Number of samples per reception
            count = 0.005 * dev.rx.samplerate;

            % Timeout value, in ms.
            timeout = 2 * dev.rx.config.timeout_ms;

            dev.rx.start();

            % Get results for each frequency in the list.
            for f = 1:length(freq_list)
                rx_freq = freq_list(f);

                if print_status
                    fprintf('Running RX DC calibration for F_RX=%d\n', rx_freq);
                end

                dev.rx.frequency = rx_freq;

                % Move TX away from RX if it is within 1 MHz, per recommendation
                % from Lime. This is intended to avoid any potential artifacts
                % resulting from the PLLs interfering with one another.
                if abs(int32(rx_freq) - int32(dev.tx.frequency)) < 1e6
                    if abs(int32(rx_freq) - 237.5e6) <= 1e6
                        dev.tx.frequency = rx_freq + 1e6;
                    else
                        dev.tx.frequency = rx_freq - 1e6;
                    end

                    %fprintf('Changed TX=%d for RX=%d to avoid PLL-induced artifacts.\n', dev.tx.frequency, dev.rx.frequency);
                end


                % DC correction values
                corr_values = [-2048 2048];
                means = zeros(1, length(corr_values));

                % Get a coarse estimate of the DC correction response
                t = dev.rx.timestamp + 0.200 * dev.rx.samplerate;
                for i = 1:length(corr_values)
                    dev.rx.corrections.dc_i = corr_values(i);
                    dev.rx.corrections.dc_q = corr_values(i);

                    [samples, t] = rx_samples(obj, t, count, t_inc);

                    means(i) = mean(samples);
                end

                % Guess the correction values that yeilds zero-crossing of the I and Q means.
                mi = real(means(end) - means(1)) / (corr_values(end) - corr_values(1));
                mq = imag(means(end) - means(1)) / (corr_values(end) - corr_values(1));

                bi = real(means(1))  - mi * corr_values(1);
                bq = imag(means(1)) - mq * corr_values(1);

                corr_guess_i = -bi/mi;
                corr_guess_q = -bq/mq;

                % Ensure guesses are a multiple of 32 to avoid confusion
                % over the fact that values under 32 yield the same result
                corr_guess_i = int16(corr_guess_i) / 32 * 32;
                corr_guess_q = int16(corr_guess_q) / 32 * 32;

                guess_min = min(corr_guess_i, corr_guess_q);
                guess_min = max(-2048, guess_min);

                guess_max = max(corr_guess_i, corr_guess_q);
                guess_max = min(guess_max, 2048);

                range_low  = max(-2048, guess_min - 32 * 8);
                range_high = min(2048,  guess_max + 32 * 8);

                %fprintf('Guesses: Icorr=%d, Qcorr=%d\n', corr_guess_i, corr_guess_q);
                %fprintf('Searching %d to %d.\n', range_low, range_high);

                %figure; plot(corr_values, real(means), 'b-', corr_values, imag(means), 'r-', corr_guess_i, 0, 'bo', corr_guess_q, 0, 'ro');

                % Try finer guesses
                corr_values = range_low:32:range_high;
                means = zeros(1, length(corr_values));

                t = dev.rx.timestamp + 0.050 * dev.rx.samplerate;

                for i = 1:length(corr_values)
                    dev.rx.corrections.dc_i = corr_values(i);
                    dev.rx.corrections.dc_q = corr_values(i);

                    samples = dev.receive(count, timeout, t);
                    t = t + count + t_inc;

                    means(i) = mean(samples);
                end

                %figure; plot(corr_values, real(means), 'b-', corr_values, imag(means), 'r-');

                [i_err, i_idx] = min(abs(real(means)));
                [q_err, q_idx] = min(abs(imag(means)));

                i_corr = round(corr_values(i_idx));
                q_corr = round(corr_values(q_idx));

                % Store results
                results(f, 1) = i_corr;
                results(f, 2) = q_corr;
                results(f, 3) = i_err;
                results(f, 4) = q_err;
            end

            dev.rx.stop();

            % Restore settings
            dev.rx.samplerate = backup.samplerate;
            dev.rx.bandwidth  = backup.bandwidth;
            dev.rx.config     = backup.config;

            if dev.tx.frequency ~= backup.tx_freq
                dev.tx.frequency = backup.tx_freq;
            end
        end

        function results = tx_auto_dc(obj, freq_list, print_status, external_loopback)
            dev = obj.bladerf;
            results = zeros(length(freq_list), 1);

            % Change to true to view plots showing the cal sweeps
            show_debug_plots = false;

            % Backup settings before we change anything
            backup.rx.frequency  = dev.rx.frequency;
            backup.rx.samplerate = dev.rx.samplerate;
            backup.rx.bandwidth  = dev.rx.bandwidth;
            backup.rx.config     = dev.rx.config;

            backup.rx.lna        = obj.tx_dc_cal_lna;
            backup.rx.vga1       = obj.tx_dc_cal_rxvga1;
            backup.rx.vga2       = obj.tx_dc_cal_rxvga2;

            backup.tx.samplerate = dev.tx.samplerate;
            backup.tx.bandwidth  = dev.tx.bandwidth;
            backup.tx.config     = dev.tx.config;

            backup.loopback      = dev.loopback;

            % Apply settings used for calibration

            % Select a sample rate that's low enough to support USB 2.0 on weaker
            % machines, while ensuring our Fs/4 offset yields ample separation
            % between the RX and TX PLLs.
            Fs                   = obj.tx_dc_cal_samplerate;
            dev.rx.samplerate    = Fs;
            dev.tx.samplerate    = Fs;

            dev.rx.bandwidth     = obj.tx_dc_cal_rx_bw;
            dev.tx.bandwidth     = obj.tx_dc_cal_tx_bw;

            % This will set based upon the TX frequency
            curr_loopback        = dev.loopback;

            % Offset between RX and TX frequencies
            f_offset             = Fs/4;

            % Transmit somcle zeros and allow TX to underrun. When the undderrun
            % occurrs, the DAC will be held at 0+0j. We can then use RX to measure
            % the change in the magnitude of the LO leakage as we adjust TX DC
            % offset correction parameters.
            dev.tx.config.num_buffers   = 2;
            dev.tx.config.buffer_size   = 4096;
            dev.tx.config.num_transfers = 1;
            dev.tx.config.timeout_ms    = 5000;

            dev.tx.start();
            dev.transmit(zeros(dev.tx.config.buffer_size, 1));

            dev.rx.config.num_buffers   = 64;
            dev.rx.config.buffer_size   = 16384;
            dev.rx.config.num_transfers = 32;
            dev.rx.config.timeout_ms    = 5000;

            dev.rx.start();

            for f = 1:length(freq_list)
                tx_freq = int32(freq_list(f));

                % Apply desired TX frequency
                dev.tx.frequency = tx_freq;

                if print_status
                    fprintf('Running TX DC calibration for F_TX=%d\n', tx_freq);
                end

                % Adjust selected loopback mode, if needed.
                if external_loopback == false
                    if dev.tx.frequency < 1.5e9
                        if strcmpi(curr_loopback, 'RF_LNA1') == false
                            curr_loopback = 'RF_LNA1';
                            dev.loopback = curr_loopback;
                            %fprintf('Setting loopback to RF_LNA1\n');
                        end
                    elseif dev.tx.frequency >= 1.5e9
                        if strcmpi(curr_loopback, 'RF_LNA2') == false
                            curr_loopback = 'RF_LNA2';
                            dev.loopback = curr_loopback;
                            %fprintf('Setting loopback to RF_LNA2\n');
                        end
                    end
                end

                % Set RX to desired offest
                if abs(tx_freq - 237.5e6) < f_offset
                    rx_freq = tx_freq + f_offset;
                    obj.rx_tuned_low = false;
                else
                    rx_freq = tx_freq - f_offset;
                    obj.rx_tuned_low = true;
                end

                dev.rx.frequency = rx_freq;

                %fprintf('Set F_RX=%d\n', rx_freq);

                % Start with the Q correction zeroed while we work on I
                dev.tx.corrections.dc_q = 0;

                % Set our first read into the future
                t = dev.rx.timestamp + 0.125 * Fs;

                % Search for and apply an initial I correction value
                [i_corr, i_err, t] = obj.get_tx_corr(t, true, print_status, show_debug_plots);
                dev.tx.corrections.dc_i = i_corr;

                if print_status
                    fprintf('  Initial I correction: %d (Error: %d)\n', i_corr, i_err);
                end

                % Now do the same for Q...
                [q_corr, q_err, t] = obj.get_tx_corr(t, false, print_status, show_debug_plots);
                dev.tx.corrections.dc_q = q_corr;

                if print_status
                    fprintf('  Q correction: %d (Error: %d)\n', q_corr, q_err);
                end

                % Redo and refine our I channel estimate that we've corrected Q
                [i_corr, i_err] = obj.get_tx_corr(t, true, print_status, show_debug_plots);
                dev.tx.corrections.dc_i = i_corr;

                if print_status
                    fprintf('  Final I correction: %d (Error: %d)\n', i_corr, i_err);
                end

                % Store results
                results(f, 1) = i_corr;
                results(f, 2) = q_corr;
                results(f, 3) = i_err;
                results(f, 4) = q_err;
            end

            dev.tx.stop();
            dev.rx.stop();


            % Restore settings
            dev.loopback      = backup.loopback();

            dev.tx.samplerate = backup.tx.samplerate;
            dev.tx.bandwidth  = backup.tx.bandwidth;
            dev.tx.config     = backup.tx.config;

            dev.rx.frequency  = backup.rx.frequency;
            dev.rx.samplerate = backup.rx.samplerate;
            dev.rx.bandwidth  = backup.rx.bandwidth;
            dev.rx.config     = backup.rx.config;

            dev.rx.lna        = backup.rx.lna;
            dev.rx.vga1       = backup.rx.vga1;
            dev.rx.vga2       = backup.rx.vga2;

        end


        function [avg_mag, t_next] = get_tx_mag(obj, t_rx)
            n_inc = 0.025 * bladeRF_IQCorr.tx_dc_cal_samplerate;
            count = 0.005 * bladeRF_IQCorr.tx_dc_cal_samplerate;

            [samples, t_next] = rx_samples(obj, t_rx, count, n_inc);

            samples = obj.fs4_mix(samples, obj.rx_tuned_low);
            samples = filter(obj.tx_cal_filt, 1, samples);

            avg_mag = mean(abs(samples));
        end

        function [corr_val, err_val, t_next] = get_tx_corr(obj, t, i_channel, print_status, show_plot)
            if nargin < 5
                show_plot = false;
            end

            dev = obj.bladerf;

            % Get a coarse estimate of the correction by finding the
            % intersect          % Get a coarse estimate of the correction by finding the
            % intersection of lines formed by x1->x2 and x3->x4
            %
            %             x1    x2   x3   x4
            dc_corr = [ -1800 -1000 1000 1800];
            mag = zeros(length(dc_corr), 1);

            for c = 1:length(dc_corr)
                if i_channel
                    dev.tx.corrections.dc_i = dc_corr(c);
                else
                    dev.tx.corrections.dc_q = dc_corr(c);
                end

                [mag(c), t] = obj.get_tx_mag(t);
            end

            m1 = (mag(2) - mag(1)) / (dc_corr(2) - dc_corr(1));
            m2 = (mag(4) - mag(3)) / (dc_corr(4) - dc_corr(3));

            b1 = mag(1) - m1 * dc_corr(1);
            b2 = mag(3) - m2 * dc_corr(3);

            if m1 < 0 && m2 > 0
                % Normal, expected case
                corr_est = round((b2 - b1) / (m1 - m2));

                % Read n_sweep points on either side of our estimate
                n_sweep = 10;
                range_min = max(-2048, corr_est - n_sweep * 16);
                range_max = min(2048,  corr_est + n_sweep * 16);

                corr_sweep = (range_min:16:range_max);
            else
                % The frequency & gain combination doesn't allow us
                % to pull the DC offset to zero. Ideally, we could adjust
                % our RX gain settings to get a better estimate.
                %
                % We'll do a slow scan to at least try to hit a
                % minimum.local minimum
                if print_status == true
                    fprintf('Performing full sweep because no turning-point was found.\n');
                    fprintf(' m1=%f, b1=%f, m2=%f, b2=%f\n', m1, b1, m2, b2);
                end

                corr_sweep = (-2048 : 16 : 2048);
                corr_est = Inf;
            end

            % Sweep nearby points to find the minimum
            sweep_mags = zeros(length(corr_sweep), 1);
            for c = 1:length(corr_sweep)
                if i_channel
                    dev.tx.corrections.dc_i = corr_sweep(c);
                else
                    dev.tx.corrections.dc_q = corr_sweep(c);
                end
                [sweep_mags(c), t] = obj.get_tx_mag(t);
            end

            % Identify the minimum
            [err_val, min_idx] = min(sweep_mags);
            corr_val = corr_sweep(min_idx);

            % Debug hook for viewing sample points
            if show_plot
                if i_channel
                    title_text = 'I';
                else
                    title_text = 'Q';
                end

                title_text = [title_text ...
                              ' DC correction sweep\newlineEstimate=' num2str(corr_est) ...
                              ', Result=' num2str(corr_val) ...
                              ', Error=' num2str(err_val)];

                figure;
                plot(corr_sweep, sweep_mags, 'b.', dc_corr, mag, 'rx', corr_val, err_val, 'co');
                title(title_text);
                xlabel('Correction Values');
                ylabel('Sample value');

                % Re-read and advance timestamp to accout for time we spent
                % creating the plot.
                t = dev.rx.timestamp + 0.5 * dev.rx.samplerate;
            end

            % Return updated read timestamp
            t_next = t;
        end
    end
end
