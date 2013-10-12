/*
 * Copyright (c) 2013 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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
#include "spi_flash_lib.h"


/* SPI initialization for application. */
CyU3PReturnStatus_t CyFxSpiInit(void)
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
	return CyU3PSpiDeInit();
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

CyBool_t spiFastRead = CyFalse;
void CyFxSpiFastRead(CyBool_t v) {
    spiFastRead = v;
}

/* SPI read / write for programmer application. */
CyU3PReturnStatus_t CyFxSpiTransfer(uint16_t pageAddress, uint16_t byteCount, uint8_t *buffer, CyBool_t isRead)
{
    uint8_t location[4];
    uint32_t byteAddress = 0;
    uint16_t pageCount = (byteCount / FLASH_PAGE_SIZE);
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    if (byteCount == 0) {
        return CY_U3P_SUCCESS;
    }

    if ((byteCount % FLASH_PAGE_SIZE) != 0) {
        return CY_U3P_ERROR_NOT_SUPPORTED;
    }

    byteAddress = pageAddress * FLASH_PAGE_SIZE;

    while (pageCount != 0) {
        location[1] = (byteAddress >> 16) & 0xFF;       /* MS byte */
        location[2] = (byteAddress >> 8) & 0xFF;
        location[3] = byteAddress & 0xFF;               /* LS byte */

        if (isRead) {
            location[0] = 0x03; /* Read command. */

            if (!spiFastRead) {
                status = CyFxSpiWaitForStatus();
                if (status != CY_U3P_SUCCESS)
                    return status;
            }

            CyU3PSpiSetSsnLine(CyFalse);
            status = CyU3PSpiTransmitWords(location, 4);

            if (status != CY_U3P_SUCCESS) {
                CyU3PDebugPrint (2, "SPI READ command failed\r\n");
                CyU3PSpiSetSsnLine (CyTrue);
                return status;
            }

            status = CyU3PSpiReceiveWords(buffer, FLASH_PAGE_SIZE);
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

            status = CyU3PSpiTransmitWords(buffer, FLASH_PAGE_SIZE);
            if (status != CY_U3P_SUCCESS) {
                CyU3PSpiSetSsnLine(CyTrue);
                return status;
            }

            CyU3PSpiSetSsnLine(CyTrue);
        }

        byteAddress += FLASH_PAGE_SIZE;
        buffer += FLASH_PAGE_SIZE;
        pageCount--;

        if (!spiFastRead)
            CyU3PThreadSleep(15);
    }

    return CY_U3P_SUCCESS;
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
