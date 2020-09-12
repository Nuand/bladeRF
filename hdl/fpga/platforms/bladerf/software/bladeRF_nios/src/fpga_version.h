/* This file defines the FPGA version number.
 * In the future, this could/should moved to a QSYS port */
#ifndef BLADERF_FPGA_VERSION_H_
#define BLADERF_FPGA_VERSION_H_

#include <stdint.h>

#define FPGA_VERSION_ID         0x7777
#define FPGA_VERSION_MAJOR      0
#define FPGA_VERSION_MINOR      11
#define FPGA_VERSION_PATCH      1
#define FPGA_VERSION ((uint32_t)( FPGA_VERSION_MAJOR        | \
                                 (FPGA_VERSION_MINOR << 8)  | \
                                 (FPGA_VERSION_PATCH << 16) ) )
#endif
