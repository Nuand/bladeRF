dev = bladeRF();

Fc = 450e6;
Fs = 5e6;

n_sec          = 3;
num_rxs        = 256;
samples_per_rx = floor(Fs * n_sec / num_rxs);

rxs = zeros(num_rxs, samples_per_rx);

dev.rx.config.num_buffers = 512;
dev.rx.frequency  = Fc;
dev.rx.samplerate = Fs;
dev.rx.bandwidth  = 1.5e6;

fprintf('Running with the following settings:\n');
dev.rx
dev.rx.config

dev.rx.start();

for n = 1:size(rxs, 1)
   [rxs(n,:), ~, actual, underrun] = dev.receive(samples_per_rx);

   if underrun
       fprintf('Underrun on iteration %d: Got %d / %d samples.\n', ...
               n, actual, samples_per_rx);
   end
end

dev.rx.stop();
clear dev;

fprintf('Done. Plotting results...\n');
rxs = rxs.';
samples = rxs(:);
clear rxs;
%pwelch(samples, [], [], [], Fs, 'centered');
plot(abs(samples));

