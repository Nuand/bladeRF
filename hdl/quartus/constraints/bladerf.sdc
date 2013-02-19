# Clock inputs
create_clock -period "100.0 MHz" [get_ports fx3_pclk]
create_clock -period "38.4 MHz"  [get_ports c4_clock]

# Generate the appropriate PLL clocks
derive_pll_clocks
dervice_clock_uncertainty

# TODO: Constrain the FX3 interface

# TODO: Constrain the LMS interface
