%-------------------------------------------------------------------------
% fsk_csv.m: MATLAB implementation of PHY portion of CPFSK modem, which
% writes TX samples to a file and reads RX samples from a file. When compared
% to the C modem this code has the following differences:
%   1) Simpler/less rigorous power normalization
%   2) No scrambling.
%
% Notes: In order to run this successfully you need the save_csv() and
% load_csv() functions from the bladeRF repository. If you have the repository
% cloned on your local machine, simply add bladeRF/host/misc/matlab to your
% MATLAB path.
% Change 'null_amt' for different amount of flat samples placed at
% front/back of tx waveform
%
% This file is part of the bladeRF project
%
% Copyright (C) 2016 Nuand LLC
%
% This program is free software; you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation; either version 2 of the License, or
% (at your option) any later version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License along
% with this program; if not, write to the Free Software Foundation, Inc.,
% 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
%-------------------------------------------------------------------------

clear;                %Clear workspace variables
txfile = 'tx_samples.csv';
rxfile = 'rx_samples.csv';

Fs = 2e6;            %Sample rate of 2 Msps (500ns sample period)
samps_per_symb = 8;  %Samples per symbol
h = pi/2;            %Modulation index (phase deviation per symbol)
                     %Note: preamble has been optimized for h = pi/2
dec_factor = 2;      %Amount to decimate by when performing correlation
                     %with preamble

%32 bit training sequence
%hex: AA, AA, AA, AA
%Note: In order for the preamble waveform not to be messed up, the last
%sample of the modulated training sequence MUST be 1 + 0j
training_seq = ['10101010';
                '10101010';
                '10101010';
                '10101010'];
%32-bit preamble - hex: 2E, 69, 2C, F0
preamble = ['00101110'; ...
            '01101001'; ...
            '00101100'; ...
            '11110000'];

%%-------------------TRANSMIT-----------------------------
%Amount of flat samples (0+0j) to add to the beginning/end of tx signal
null_amt = 200;

%Prompt for string to transmit
prompt = 'Enter a string to tx: ';
str = input(prompt, 's');
%Convert to bit matrix
tx_bits = dec2bin(uint8(str), 8);

%Make FSK signal
tx_sig = fsk_transmit(training_seq, preamble, tx_bits, samps_per_symb, h);
%Add some flat output to the front and back of the signal
tx_sig(null_amt+1:length(tx_sig)+null_amt) = tx_sig;
tx_sig(1:null_amt) = 0 + 0j;
tx_sig(length(tx_sig)+1:length(tx_sig)+null_amt) = 0 + 0j;

%time vector: 1/Fs increments
t = 0 : 1/Fs : (length(tx_sig)-1)/Fs;

%Write to csv file
save_csv(txfile, tx_sig.');

%TX IQ samples
figure('position', [960 775 960 200]);
plot(t, real(tx_sig), 'Color', [0,.447,.741]);
hold on;
plot(t, imag(tx_sig), 'Color', [.85,.325,.098]);
title('TX IQ samples vs time');

%Wait for user to transmit/receive these samples with bladeRF
disp('Press any key when rx samples file is ready...');
pause;

%%---------------------RECEIVE---------------------------
%Demodulate the set of samples given in 'rxfile' and print the received
%string.
%Load csv from file
rx_sig = load_csv(rxfile).';
%Get the modulated IQ waveform for the preamble
preamble_waveform = fsk_mod(preamble, samps_per_symb, h, 0);
%Filter/Normalize/Detect/Demodulate FSK signal
[rx_bits, rx_info] = fsk_receive(preamble_waveform, rx_sig, dec_factor, ...
                                    samps_per_symb, h, size(tx_bits, 1));

%----------------------Plot some stuff---------------------
%time vector: 1/Fs increments
t = 0 : 1/Fs : (length(rx_sig)-1)/Fs;

%RX IQ samples
figure('position', [960 525 960 200]);
plot(t, real(rx_sig), 'Color', [0,.447,.741]);
hold on;
plot(t, imag(rx_sig), 'Color', [.85,.325,.098]);
ylim([-1, 1]);
title('RX raw IQ samples vs time');

%RX raw spectrum
figure('position', [960 275 960 200]);
if (exist('OCTAVE_VERSION', 'builtin') ~= 0)
	pwelch(rx_sig, [], [], 256, Fs, 'centerdc');
else
	pwelch(rx_sig, [], [], [], Fs, 'centered');
end
title('RX raw spectrum');

%RX filtered IQ samples
figure('position', [960 0 960 200]);
plot(real(rx_info.iq_filt_norm), 'Color', [0,.447,.741]);
hold on;
plot(imag(rx_info.iq_filt_norm), 'Color', [.85,.325,.098]);
title('RX filtered & normalized IQ samples');

%RX cross correlation with preamble. Plot power (i^2 + q^2).
figure('position', [0 775 960 200]);
plot(abs(rx_info.preamble_corr).^2);
title('RX cross correlation with preamble (power)');

%RX dphase (data signal only, training/preamble not included)
if (rx_info.dphase ~= -1)
    figure('position', [0 500 960, 200]);
    plot(rx_info.dphase);
    title('RX dphase (data signal only)');
end

%---------------------Print received data-------------------
if (rx_bits(1) ~= -1)
    fprintf('Received: ''%s''\n', bin2dec(rx_bits));
    if isequal(rx_bits, tx_bits)
        fprintf('RX data matched TX data\n');
    else
        fprintf(2, 'RX data did not match TX data\n');
    end
end

fprintf('Press any key to close figures...\n');
pause;
close all;
