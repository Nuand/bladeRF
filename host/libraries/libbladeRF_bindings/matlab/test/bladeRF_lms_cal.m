% Run LMS calibration routines
%
% bladeRF_lms_cal(device, rx_freq, tx_freq)
%
%   device  - Optional. May be an existing handle or a device specification string
%   rx_freq - Optional. RX frequency to set the device to. Default = 910e6.
%   tx_freq - Optional. TX frequency to set the device to. Default = 915e6.
function bladeRF_lms_cal(device, rx_freq, tx_freq)
    if nargin >= 1 && ischar(device)
        b = bladeRF(device);
    else
        b = device;
    end

    if nargin < 2
        rx_freq = 910e6;
    end

    if nargin < 3
        tx_freq = 915e6;
    end

    figure;

    b.rx.frequency = rx_freq;
    b.tx.frequency = tx_freq;

    overrun = true;
    retry = 0;

    % Get an initial sample set
    disp('Getting pre-cal samples...')
    b.rx.start();
    while overrun == true && retry < 3
        [s1, ~, ~, overrun]  = b.receive(b.rx.samplerate / 4);
        retry = retry + 1;
    end
    b.rx.stop();

    if overrun == true
        error('Failed to run test - too many overruns.');
    end


    subplot(2, 2, 1);
    plot(s1, 'b.');

    subplot(2, 2, 2);
    plot(1:length(s1), real(s1), 1:length(s1), imag(s1));

    % Run all calibrations
    disp('Running calibrations...')
    b.calibrate('ALL');

    % Get a second sample set
    disp('Getting post-cal samples...')
    overrun = true;
    retry = 0;
    b.rx.start();
    while overrun == true && retry < 3
        [s2, ~, ~, overrun]  = b.receive(b.rx.samplerate / 4);
        retry = retry + 1;
    end
    b.rx.stop();

    if overrun == true
        error('Failed to run test - too many overruns.');
    end

    subplot(2, 2, 3);
    plot(s2, 'r.');

    subplot(2, 2, 4);
    plot(1:length(s2), real(s2), 1:length(s2), imag(s2));

    % Run each calibration just to ensure the API isn't broken.
    disp('Testing individual cal routines...');
    b.calibrate('LPF_TUNING');
    b.calibrate('RX_LPF');
    b.calibrate('RXVGA2');
    b.calibrate('TX_LPF');

    hold off;
    clear b;
end
