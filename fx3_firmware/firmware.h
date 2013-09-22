#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_

#include <stddef.h>
#include "cyu3error.h"

CyU3PReturnStatus_t NuandReadOtp(size_t offset, size_t size, void *buf);
CyU3PReturnStatus_t NuandWriteOtp(size_t offset, size_t size, void *buf);
CyU3PReturnStatus_t NuandLockOtp();

void NuandFirmwareStart();
void NuandFirmwareStop();

int NuandExtractField(char *ptr, int len, char *field,
                            char *val, size_t  maxlen);

#endif /* _FIRMWARE_H_ */
