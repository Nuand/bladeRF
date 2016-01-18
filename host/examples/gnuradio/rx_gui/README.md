# GNU Radio Companion RX GUI Example #

This flowgraph is intended to serve as a simple RX template for bladeRF-based GRC projects.
It exemplifies a number of the features provided by the `osmocom Source` block, and how these
features map to functionality provided by the underlying libbladeRF library.

The various parameters are exposed to the command-line via `Parameter` blocks.
The values of these blocks are used to:
 * Set initial values in each of the GUI widgets.  
 * Construct the `Device Arguments` string provided to the `osmocom Source`
 
