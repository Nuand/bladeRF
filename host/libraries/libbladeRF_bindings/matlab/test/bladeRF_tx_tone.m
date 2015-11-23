function bladeRF_tx_tone(tx_mode, Fc, Fs, bandwidth, Ftone, num_seconds, devstr)
% bladeRF_test_tx_tone  This function transmits a tone using one of three
% transmit "modes."
%
% bladeRF_tx_tone(tx_mode, Fc, Fs, bandwidth, Ftone, num_seconds, devstr)
%
% Input Parameters:
%   tx_mode          One of: 'single_burst', 'split burst', 'stream'
%   Fc               Center frequency
%   Fs               Sample rate
%   bandwidth        LPF bandwidth
%   Ftone            Tone frequency
%   num_seconds      # of seconds to transmit tone for
%   devstr           Device string

%% Handle args
if nargin < 1
    tx_mode = 'single burst';
else
    tx_mode = lower(tx_mode);
end

if nargin < 2
    Fc = 915e6;
end

if nargin < 3
    Fs = 5e6;
end

if nargin < 4
    bandwidth = 1.5e6;
end

if nargin < 5
    Ftone = 250e3;
end

if nargin < 6
    num_seconds = 5;
end

if nargin < 7
    devstr = '';
end

%% Craft tone samples ahead of time
fprintf('Creating %f seconds of a %f Hz tone.\n', num_seconds, Ftone);

omega = 2 * pi * Ftone;
t = 0 : 1/Fs : num_seconds;
sig = 0.5 .* exp(1j * omega * t);


%% Setup device
dev = bladeRF(devstr);

dev.tx.frequency  = Fc;
dev.tx.samplerate = Fs;
dev.tx.bandwidth  = bandwidth;
dev.tx.vga2 = 0;
dev.tx.vga1 = -20;

dev.tx.config.num_buffers = 64;
dev.tx.config.buffer_size = 16384;
dev.tx.config.num_transfers = 16;

fprintf('Running with the following settings:\n');
disp(dev.tx)
disp(dev.tx.config)


% Transmit the entire signal as a "burst" with some zeros padded on either
% end of it.
dev.tx.start();

switch tx_mode
    %% Send samples as a single burst
    case 'single burst'
    dev.transmit(sig, 2 * num_seconds, 0, true, true);


    %% Send samples 3 times, but as a single burst split across 3 calls
    case 'split burst'
    end_i = round(10 * dev.tx.config.buffer_size);
    dev.transmit(sig(1:end_i), num_seconds, 0, true, false);

    start_i = end_i + 1;
    end_i = round(length(sig) * 2 / 3);
    dev.transmit(sig(start_i:end_i), num_seconds, 0, false, false);

    start_i = end_i + 1;
    dev.transmit(sig(start_i:end), num_seconds, 0, false, true);

    %% Send samples via a stream-like use case.
    case 'stream'
        end_i = round(length(sig) / 3);
        dev.transmit(sig(1:end_i));

        start_i = end_i + 1;
        end_i = round(length(sig) * 2 / 3);
        dev.transmit(sig(start_i:end_i));

        start_i = end_i + 1;
        dev.transmit(sig(start_i:end));

        % Send a few zeros through to ensure we hold the TX DAC to 0+0j until
        % we shut down.
        dev.transmit(zeros(10, 1));

    otherwise
        clear dev;
        error(['Invalid test mode specified: ' tx_mode]);
end


%% Shutdown and cleanup

disp('Waiting for transmission to complete.');
tic

% Wait to ensure all of our samples have reached the RF frontend.
% First, figure out how long would take to transmit every remaining buffer
max_buffered = (dev.tx.config.num_buffers * dev.tx.config.buffer_size);

t = dev.tx.timestamp + max_buffered;

while t > dev.tx.timestamp
    pause(0.250);
end

disp('Shutting down.');

dev.tx.stop();
clear dev Fs Fc Ftone num_seconds t omega sig start_i end_i max_buffered;
