% Receive every 250 ms

dev = bladeRF();

Fc = 450e6;
Fs = 5e6;



samples_per_rx = 0.010 * Fs;
num_rxs        = 10;

num_samples    = floor(samples_per_rx * num_rxs);
samples = zeros(num_samples, 1);

dev.rx.config.num_buffers = 512;
dev.rx.frequency  = Fc;
dev.rx.samplerate = Fs;
dev.rx.bandwidth  = 1.5e6;

fprintf('Running with the following settings:\n');
dev.rx
dev.rx.config

dev.rx.start();

% Get the current time and advance it by 1 ms
timestamp = dev.rx.timestamp + round(0.001 * Fs);

for n = 1:num_rxs
   i_start = (n - 1) * samples_per_rx + 1;
   i_end   = i_start + samples_per_rx - 1;

   [samples(i_start:i_end), ~, actual, underrun] = ...
       dev.receive(samples_per_rx, 5000, timestamp);

   if underrun
       fprintf('Underrun on iteration %d/%d: Got %d / %d samples.\n', ...
               n, num_rxs, actual, samples_per_rx);
   else
       %fprintf('Finished iteration %d/%d (ts=%d)\n', n, num_rxs, timestamp);
   end

   timestamp = timestamp + samples_per_rx + round(0.001 * Fs);
end

dev.rx.stop();
clear dev;

fprintf('Done. Plotting results...\n');
plot(1:length(samples), real(samples));

