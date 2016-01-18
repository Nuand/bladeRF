# GNU Radio Companion Loopback Example #

This flowgraph illustrates the use of the `loopback=<mode>` parameter that may
be provided to an `osmocom Source` block to internally connect a bladeRF's TX
signal path to its RX path.

This is intended to allow users to experiment with the effect of various
parameters for different loopback modes. For example, The TX VGA1 gain has
no effect when a loopback mode that selects the output of the TXLPF is
selected. It is *highly* recommended that users refer to the LMS6002D block
diagram in the [LMS6002D data sheet](http://www.limemicro.com/products/field-programmable-rf-ics-lms6002d/#resources)
to better understand the available loopback paths.

The available loopback mode settings are listed on the [gr-osmosdr wiki page](http://sdr.osmocom.org/trac/wiki/GrOsmoSDR#bladeRFSourceSink).
One may also reference the descriptions of the associated `bladerf_loopback`
enumeration values in the [libbladeRF API documentation](https://nuand.com/bladeRF-doc/libbladeRF).
