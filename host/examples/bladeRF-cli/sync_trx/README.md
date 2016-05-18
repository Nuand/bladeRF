# Synchronized TRX #

This example illustrates how to synchronize RX and TX sampling on
multiple devices, using the bladeRF-cli and the trigger functionality
introduced in FPGA v0.6.0.

This example assumes the use of an RF power splitter/combiner and two bladeRF
devices, connected as follows:

1. The "master" bladeRF has both its TX and RX ports connected to
   the combiner.

2. The "slave" bladeRF has its RX port connected to the combiner.

3. Pin 4 on J71 on both devices is connected together. This connects
   the trigger signal on both devices. The "master" will output this
   signal, and the "slave" will receive it.

4. Pin 2 on J71 on both devices is connected. This provides a common ground.

5. The J62 SMB clock connector of both devices are connected together.
   The "master" bladeRF will output its 38.4 MHz reference clock, and the
   "slave" bladeRF will utilize this reference instead of its own on-board
   clock. This ensures that both devices are sampling using the same clock.

When a trigger is "armed," the FPGA will gate both transmit and receive
samples; the flow of samples to/from the host is blocked. The assertion
("firing") of the trigger signal unblocks samples, causing all devices to
begin transmitting or receiving within +/- 1 sample clock period.

In this example, the "master" will transmit a series of on-off pulses at a
1 MHz offset, and both the "master" and "slave" will receive these pulses. The
transmission and reception are synchronized using the J71-4 trigger signal.

## Creating Samples ##

Scripts are provided to create a samples.csv file containing a single pulse.
This files consists of 1,000 zeros, followed by 100 samples of a CW tone,
followed by 1,000 more zeros.

In Linux and OSX, run `create_samples.sh`. In Windows, run `create_samples.bat`.
This script make take a few moments to complete.

## Executing the Example Scripts ##

Two bladeRF-cli scripts, `master.txt` and `slave.txt`, are provided for this
example.

The `master.txt` script configures the "master" device to output its
reference clock on the SMB connector (J62), arm a TX "master" trigger, and
arm an RX "slave" trigger.

Note that even though both the RX and TX paths are on the same device, one
of these must assume the "master" trigger role that will output the trigger signal.
Internally, the other path with receive this -- it is *very* important
that both not be configured as "master", as this would cause contention
over the trigger signal.

In a multi-device configuration, especially for this master-slave setup, it is
recommended to run the bladeRF-cli in a manner that specifies the device
to open via serial number.

Run `bladeRF-cli -p` to see a list of available devices. For example:

```
$ bladeRF-cli -p

Backend:        libusb
Serial:         e93f190c76e987cd7f462026f965a1a1
USB Bus:        1
USB Address:    13

Backend:        libusb
Serial:         b74ed347390d35c29923e80f64c9d904
USB Bus:        12
USB Address:    8

```

For this example, the "master" is the device with serial number `b57...`
and the "slave" is the device with serial number `e93...`.

In one terminal, run the `master.txt` script:

```
$ bladeRF -d '*:serial=b74' -s master.txt -i
```

In another terminal, run the `slave.txt` script:

```
$ bladeRF -d '*:serial=e93' -s slave.txt
```

At this point, both devices' sample streams are blocked and awaiting
a trigger. In the "master" console, fire the trigger via:

```
trigger j71-4 tx fire
```

Wait for the RX and TX operations complete via the commands:

```
rx wait; tx wait
```

It will most likely be the case that these operations have finished
before you can type these commands. They will return immediately if
this is true.

## Viewing the Results ##

The results may be viewed using [Octave] [1] or [MATLAB] [2].

Run one of these programs in this directory and execute the
`plot_samples` function provided in this directory. This script
will load and plot samples from the `master_tx.csv`, `master_rx.csv`, and 
`slave_rx.csv` files located in the current working directory.

Note that in the resulting plot, the first pulse transmitted by the "master"
will be red/orange.  The signal received by the "master" device is blue. The
signal received by the "slave" device is green.

Zoom in to see the first few pulses in detail. The reception of the pulse by
the "master" and "slave" devices should be synchronized within +/- 1 samples.

Although the devices' sampling clocks are synchronized, they are not guaranteed
to be phase-aligned; the mixers in each device operate using independent
PLLs. However, the phase offset between the synchronized devices should remain
fixed. This allows for opportunities to determine and compensate for
phase offsets.

Lastly, there is a fixed offset between the transmission of the signal
by the "master" device, and its reception. This length of this offset
is determined by the TX chain in the FPGA and LMS6002D, and should remain
a constant value. Thus, one can make a priori assumptions about this value
after quantifying it for a particular FPGA version. (Changes to the FPGA
code could alter this value.)

[1]: https://www.gnu.org/software/octave/ "GNU Octave"
[2]: https://www.mathworks.com/products/matlab "MATLAB"
