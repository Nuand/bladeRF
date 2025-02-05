rx config file=dual_test.bin n=10M channel=1,2
set frequency rx 915e6
set samplerate 2e6
set agc 0
set clock_ref 1
rx start
p
rx wait