#ifndef _FPGA_H_
#define _FPGA_H_

#include "bladeRF.h"

void NuandFpgaConfigSwInit(void);
extern const struct NuandApplication NuandFpgaConfig;
int FpgaBeginProgram(void);
CyBool_t NuandLoadFromFlash(int fpga_len);

#endif /* _FPGA_H_ */
