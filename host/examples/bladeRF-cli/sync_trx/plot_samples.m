% This script assumes it is being run from:
%   ${BLADERF}/host/examples/bladeRF-cli/sync_trx
%
% If this is not the case, change the following addpath() line to point to: 
%    ${BLADERF}/host/misc/matlab
% 
% This directory contains the load_csv() function.
addpath('../../../misc/matlab')

master_tx = load_csv('master_tx.csv');
master_rx = load_csv('master_rx.csv');
slave_rx  = load_csv('slave_rx.csv');

master_tx_len = length(master_tx);
master_rx_len = length(master_rx);
slave_rx_len  = length(slave_rx);

% Scale the master_tx samples to the max RX'd amplitude
% just to make the plot a bit easier to read.
abs_master_tx = abs(master_tx) * max(abs(master_rx));

plot(1:master_tx_len, abs_master_tx, 'r', ... 
     1:master_rx_len, abs(master_rx), 'g', ...
     1:slave_rx_len,  abs(slave_rx), 'b');

