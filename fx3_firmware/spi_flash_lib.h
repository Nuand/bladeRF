#ifndef _SPI_FLASH_LIB_H_
#define _SPI_FLASH_LIB_H_

#define FLASH_PAGE_SIZE 0x100  /* SPI Page size to be used for transfers. */

CyU3PReturnStatus_t CyFxSpiInit();
CyU3PReturnStatus_t CyFxSpiDeInit();
CyU3PReturnStatus_t CyFxSpiTransfer(
        uint16_t pageAddress, uint16_t byteCount,
        uint8_t *buffer, CyBool_t isRead);

#endif
