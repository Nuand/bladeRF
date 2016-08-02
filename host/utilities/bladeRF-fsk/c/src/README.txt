CODE ORGANIZATION
-----------------

bladeRF-fsk.c                                   other files:
    |___________                                utils.c - utility functions
    |           |                               common.h - common definitions
 link.c     config.c                            rx_ch_filter.h - FIR filter taps
    |___________                                test_suite.c - tests
    |           |
  phy.c       crc32.c
    |__________________________________________________________________________
    |              |           |           |          |           |            |
{libbladeRF}  radio_config.c  fsk.c   fir_filter.c  pnorm.c   correlator.c   prng.c
    |
    |
{bladeRF device}
