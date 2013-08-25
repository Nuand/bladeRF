/*
 * spi_flash_lib.c
 *
 *  Created on: Feb 13, 2013
 *      Author: Robert
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "bladeRF.h"
#include "cyu3gpif.h"
#include "cyu3pib.h"
#include "cyu3gpio.h"
#include "pib_regs.h"

#include "cyu3spi.h"
uint16_t glSpiPageSize = 0x100;  /* SPI Page size to be used for transfers. */

/* SPI initialization for application. */
CyU3PReturnStatus_t CyFxSpiInit(uint16_t pageLen)
{
    CyU3PSpiConfig_t spiConfig;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Start the SPI module and configure the master. */
    status = CyU3PSpiInit();
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    /* Start the SPI master block. Run the SPI clock at 8MHz
     * and configure the word length to 8 bits. Also configure
     * the slave select using FW. */
    CyU3PMemSet((uint8_t *)&spiConfig, 0, sizeof(spiConfig));
    spiConfig.isLsbFirst = CyFalse;
    spiConfig.cpol       = CyTrue;
    spiConfig.ssnPol     = CyFalse;
    spiConfig.cpha       = CyTrue;
    spiConfig.leadTime   = CY_U3P_SPI_SSN_LAG_LEAD_HALF_CLK;
    spiConfig.lagTime    = CY_U3P_SPI_SSN_LAG_LEAD_HALF_CLK;
    spiConfig.ssnCtrl    = CY_U3P_SPI_SSN_CTRL_FW;
    spiConfig.clock      = 8000000;
    spiConfig.wordLen    = 8;

    status = CyU3PSpiSetConfig(&spiConfig, NULL);

    return status;
}

CyU3PReturnStatus_t CyFxSpiDeInit() {
	CyU3PSpiDeInit();
}

/* Wait for the status response from the SPI flash. */
CyU3PReturnStatus_t CyFxSpiWaitForStatus(void)
{
    uint8_t buf[2], rd_buf[2];
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Wait for status response from SPI flash device. */
    do {
        buf[0] = 0x06;  /* Write enable command. */

        CyU3PSpiSetSsnLine(CyFalse);
        status = CyU3PSpiTransmitWords (buf, 1);
        CyU3PSpiSetSsnLine(CyTrue);
        if (status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint (2, "SPI WR_ENABLE command failed\n\r");
            return status;
        }

        buf[0] = 0x05;  /* Read status command */

        CyU3PSpiSetSsnLine(CyFalse);
        status = CyU3PSpiTransmitWords(buf, 1);
        if (status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(2, "SPI READ_STATUS command failed\n\r");
            CyU3PSpiSetSsnLine(CyTrue);
            return status;
        }

        status = CyU3PSpiReceiveWords(rd_buf, 2);
        CyU3PSpiSetSsnLine(CyTrue);
        if(status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(2, "SPI status read failed\n\r");
            return status;
        }

    } while ((rd_buf[0] & 1) || (!(rd_buf[0] & 0x2)));

    return CY_U3P_SUCCESS;
}

/* SPI read / write for programmer application. */
CyU3PReturnStatus_t CyFxSpiTransfer(uint16_t pageAddress, uint16_t byteCount, uint8_t *buffer, CyBool_t isRead)
{
    uint8_t location[4];
    uint32_t byteAddress = 0;
    uint16_t pageCount = (byteCount / glSpiPageSize);
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    if (byteCount == 0) {
        return CY_U3P_SUCCESS;
    }

    if ((byteCount % glSpiPageSize) != 0) {
        pageCount ++;
    }

    byteAddress = pageAddress * glSpiPageSize;

    while (pageCount != 0) {
        location[1] = (byteAddress >> 16) & 0xFF;       /* MS byte */
        location[2] = (byteAddress >> 8) & 0xFF;
        location[3] = byteAddress & 0xFF;               /* LS byte */

        if (isRead) {
            location[0] = 0x03; /* Read command. */

            status = CyFxSpiWaitForStatus();
            if (status != CY_U3P_SUCCESS)
                return status;

            CyU3PSpiSetSsnLine(CyFalse);
            status = CyU3PSpiTransmitWords(location, 4);

            if (status != CY_U3P_SUCCESS) {
                CyU3PDebugPrint (2, "SPI READ command failed\r\n");
                CyU3PSpiSetSsnLine (CyTrue);
                return status;
            }

            status = CyU3PSpiReceiveWords(buffer, glSpiPageSize);
            if (status != CY_U3P_SUCCESS) {
                CyU3PSpiSetSsnLine(CyTrue);
                return status;
            }

            CyU3PSpiSetSsnLine(CyTrue);
        } else { /* Write */
            location[0] = 0x02; /* Write command */

            status = CyFxSpiWaitForStatus();
            if (status != CY_U3P_SUCCESS)
                return status;

            CyU3PSpiSetSsnLine(CyFalse);
            status = CyU3PSpiTransmitWords(location, 4);
            if (status != CY_U3P_SUCCESS) {
                CyU3PDebugPrint(2, "SPI WRITE command failed\r\n");
                CyU3PSpiSetSsnLine(CyTrue);
                return status;
            }

            status = CyU3PSpiTransmitWords(buffer, glSpiPageSize);
            if (status != CY_U3P_SUCCESS) {
                CyU3PSpiSetSsnLine(CyTrue);
                return status;
            }

            CyU3PSpiSetSsnLine(CyTrue);
        }

        byteAddress += glSpiPageSize;
        buffer += glSpiPageSize;
        pageCount--;

        CyU3PThreadSleep(15);
    }

    return CY_U3P_SUCCESS;
}

void NuandLockOtp() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    char location[4];

    location[0] = 0x05; /* RDSTATUS */
    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiReceiveWords(location, 1);
    CyU3PSpiSetSsnLine (CyTrue);

    location[0] = 0x06;  /* Write enable. */

    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);

    location[0] = 0x05; /* RDSTATUS */
    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiReceiveWords(location, 1);
    CyU3PSpiSetSsnLine (CyTrue);

    CyU3PSpiSetSsnLine (CyFalse);
    location[0] = 0x2f;// WRSCUR
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);
    CyU3PSpiSetSsnLine (CyFalse);
    location[0] = 0x2;// WRSCUR
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);

    location[0] = 0x2b; /* RDSCUR */
    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiReceiveWords(location, 1);
    CyU3PSpiSetSsnLine (CyTrue);
}

void NuandEnso() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    char location[4];

    location[0] = 0xb1; // ENSO
    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);
}


void NuandExso() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    char location[4];

    location[0] = 0xc1; // EXSO
    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);
}

/* Function to erase SPI flash sectors. */
CyU3PReturnStatus_t CyFxSpiEraseSector(CyBool_t isErase, uint8_t sector)
{
    uint32_t temp = 0;
    uint8_t  location[4], rdBuf[2];
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    location[0] = 0x06;  /* Write enable. */

    CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    CyU3PSpiSetSsnLine (CyTrue);
    if (status != CY_U3P_SUCCESS)
        return status;

    if (isErase)
    {
        location[0] = 0xD8; /* Sector erase. */
        temp        = sector * 0x10000;
        location[1] = (temp >> 16) & 0xFF;
        location[2] = (temp >> 8) & 0xFF;
        location[3] = temp & 0xFF;

        CyU3PSpiSetSsnLine (CyFalse);
        status = CyU3PSpiTransmitWords (location, 4);
        CyU3PSpiSetSsnLine (CyTrue);
    }

    location[0] = 0x05; /* RDSTATUS */
    do {
        CyU3PSpiSetSsnLine (CyFalse);
        status = CyU3PSpiTransmitWords (location, 1);
        status = CyU3PSpiReceiveWords(rdBuf, 1);
        CyU3PSpiSetSsnLine (CyTrue);
    } while(rdBuf[0] & 1);

    return status;
}

void NuandFirmwareStart() {
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    NuandGPIOReconfigure(CyFalse, CyFalse);
    FpgaBeginProgram();

    status = CyFxSpiInit(0x100);

    glAppMode = MODE_FW_CONFIG;
}

void NuandFirmwareStop() {
	CyFxSpiDeInit();
}
