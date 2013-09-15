#ifndef _SPI_FLASH_LIB_H_
#define _SPI_FLASH_LIB_H_

CyU3PReturnStatus_t CyFxSpiInit(uint16_t pageLen);
CyU3PReturnStatus_t CyFxSpiTransfer(uint16_t pageAddress, uint16_t byteCount, uint8_t *buffer, CyBool_t isRead);

#endif
