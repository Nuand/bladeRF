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
#include <string.h>
#include "cyu3spi.h"
#include "cyu3error.h"
#include "bladeRF.h"
#include "spi_flash_lib.h"
#include "flash.h"
#include "misc.h"

#define THIS_FILE LOGGER_ID_FLASH_C

static CyU3PReturnStatus_t FlashReadStatus(uint8_t *val)
{
    int status;
    uint8_t read_status = 0x05; /* RDSTATUS */
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (&read_status, 1);
    status = CyU3PSpiReceiveWords(val, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    return status;
}

static CyU3PReturnStatus_t FlashWriteEnable(void)
{
    CyU3PReturnStatus_t status;
    uint8_t cmd = 0x06; /* Write Enable */

    status = CyU3PSpiSetSsnLine(CyFalse);
    status = CyU3PSpiTransmitWords(&cmd, 1);
    status = CyU3PSpiSetSsnLine(CyTrue);

    return status;
}

static CyU3PReturnStatus_t FlashWaitUntilReady(void)
{
    CyU3PReturnStatus_t status;
    uint8_t read_status;

    do {
        status = FlashReadStatus(&read_status);
        if (status != CY_U3P_SUCCESS) {
            return status;
        }
    } while (read_status & 0x01);

    return CY_U3P_SUCCESS;
}

CyU3PReturnStatus_t NuandLockOtpMacronix() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    uint8_t location[1];
    uint8_t read_status;

    status = FlashReadStatus(&read_status);

    location[0] = 0x06;  /* Write enable. */
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    status = FlashReadStatus(&read_status);

    location[0] = 0x2f;// WRSCUR
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    location[0] = 0x02;// Page program
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    location[0] = 0x2b; /* RDSCUR */
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiReceiveWords(location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    return status;
}

CyU3PReturnStatus_t NuandLockOtpWinbond() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    uint8_t location[2];
    uint8_t val[2];

    location[0] = 0x35; // Read Status Register 2
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiReceiveWords(val, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    val[0] |= 0x8; // Set LB-1

    status = FlashWriteEnable();
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    location[0] = 0x31; // Write Status Register 2
    location[1] = val[0];

    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 2);
    status = CyU3PSpiSetSsnLine (CyTrue);

    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    status = FlashWaitUntilReady();

    return status;
}

static CyU3PReturnStatus_t NuandLockOtpRenesas() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    uint8_t cmd[4];
    uint8_t lock_val = 0x00;
    uint8_t sr;

    cmd[0] = 0x06; /* Write Enable */
    status = CyU3PSpiSetSsnLine(CyFalse);
    status = CyU3PSpiTransmitWords(cmd, 1);
    status = CyU3PSpiSetSsnLine(CyTrue);

    cmd[0] = 0x9B; /* Program OTP Security Register */
    cmd[1] = 0x00; /* A[23:16] don't care */
    cmd[2] = 0x00; /* A[15:8], A[8]=0 */
    cmd[3] = 0xFF; /* A[7:0]: A[7]=1 (reg 1), A[6:0]=0x7F (byte 127) */

    status = CyU3PSpiSetSsnLine(CyFalse);
    status = CyU3PSpiTransmitWords(cmd, 4);
    status = CyU3PSpiTransmitWords(&lock_val, 1);
    status = CyU3PSpiSetSsnLine(CyTrue);

    /* Poll SR1 bit 0 until programming completes */
    do {
        FlashReadStatus(&sr);
    } while (sr & 0x01);

    return status;
}

CyU3PReturnStatus_t NuandLockOtp() {
    uint8_t mfn = NuandGetSPIManufacturer();
    if (mfn == 0xEF) {
        return NuandLockOtpWinbond();
    } else if (mfn == 0x1F) {
        return NuandLockOtpRenesas();
    }
    return NuandLockOtpMacronix();
}

CyU3PReturnStatus_t NuandEnso() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    uint8_t location[1];

    location[0] = 0xb1; // ENSO
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    return status;
}

CyU3PReturnStatus_t NuandExso() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    uint8_t location[1];

    location[0] = 0xc1; // EXSO
    status = CyU3PSpiSetSsnLine (CyFalse);
    status = CyU3PSpiTransmitWords (location, 1);
    status = CyU3PSpiSetSsnLine (CyTrue);

    return status;
}

void NuandEraseWBSec() {
    uint8_t location[4];

    location[0] = 0x44; // Erase security register
    location[1] = 0x0;
    location[2] = 0x1f;
    location[3] = 0x0;

    CyU3PSpiSetSsnLine (CyFalse);
    CyU3PSpiTransmitWords (location, 4);
    CyU3PSpiSetSsnLine (CyTrue);
}

static uint8_t spi_mfn[2];
static CyBool_t cached_spi_mfn = CyFalse;
void cacheSPIManufacturer() {
    uint8_t location[4];

    location[0] = 0x90; // RD Manu
    location[1] = 0x0;
    location[2] = 0x0;
    location[3] = 0x0;

    CyU3PSpiSetSsnLine (CyFalse);
    CyU3PSpiTransmitWords (location, 4);
    CyU3PSpiReceiveWords(spi_mfn, 2);
    CyU3PSpiSetSsnLine (CyTrue);
    cached_spi_mfn = CyTrue;
}

uint8_t NuandGetSPIManufacturer() {
    if (!cached_spi_mfn) {
        cacheSPIManufacturer();
    }
    return spi_mfn[0];
}

uint8_t NuandGetSPIDeviceID() {
    if (!cached_spi_mfn) {
        cacheSPIManufacturer();
    }
    return spi_mfn[1];
}

CyU3PReturnStatus_t NuandReadOtp(size_t offset, size_t size, void *buf) {
    CyU3PReturnStatus_t status;

    if (NuandGetSPIManufacturer() == 0x1F) {
        uint8_t cmd[5];

        cmd[0] = 0x4B; /* Read OTP Security Register */
        cmd[1] = 0x00; /* A[23:16] don't care */
        cmd[2] = 0x00; /* A[15:9] don't care, A[8]=0 */
        cmd[3] = 0x80; /* A[7]=1 (reg 1), A[6:0]=0 (byte 0) */
        cmd[4] = 0x00; /* dummy byte */

        CyU3PSpiSetSsnLine(CyFalse);
        CyU3PSpiTransmitWords(cmd, 5);
        status = CyU3PSpiReceiveWords(buf, 128);
        CyU3PSpiSetSsnLine(CyTrue);

        memset((uint8_t *)buf + 128, 0xFF, size > 128 ? size - 128 : 0);
        return status;
    }

    if (NuandGetSPIManufacturer() == 0xEF) {
        offset = (1 << 12) / FLASH_PAGE_SIZE;
    } else {
        status = NuandEnso();
    }

    status = CyFxSpiTransfer(offset, size, buf, CyTrue, CyTrue);

    if (NuandGetSPIManufacturer() != 0xEF) {
        status = NuandExso();
    }

    return status;
}

CyU3PReturnStatus_t NuandWriteOtp(size_t offset, size_t size, void *buf) {
    CyU3PReturnStatus_t status;

    if (NuandGetSPIManufacturer() == 0x1F) {
        uint8_t cmd[4];
        uint8_t sr;
        /* Cap at 127 bytes; byte 127 is the lock byte */
        size_t write_len = size < 127 ? size : 127;

        cmd[0] = 0x06; /* Write Enable */
        CyU3PSpiSetSsnLine(CyFalse);
        CyU3PSpiTransmitWords(cmd, 1);
        CyU3PSpiSetSsnLine(CyTrue);

        cmd[0] = 0x9B; /* Program OTP Security Register */
        cmd[1] = 0x00; /* A[23:16] */
        cmd[2] = 0x00; /* A[15:8], A[8]=0 */
        cmd[3] = 0x80; /* A[7]=1 (reg 1), A[6:0]=0 */

        CyU3PSpiSetSsnLine(CyFalse);
        CyU3PSpiTransmitWords(cmd, 4);
        status = CyU3PSpiTransmitWords(buf, write_len);
        CyU3PSpiSetSsnLine(CyTrue);

        /* Poll SR1 bit 0 until programming completes */
        do {
            FlashReadStatus(&sr);
        } while (sr & 0x01);

        return status;
    }

    if (NuandGetSPIManufacturer() == 0xEF) {
        NuandEraseWBSec();
        offset = (1 << 12) / FLASH_PAGE_SIZE;
    } else {
        status = NuandEnso();
    }

    status = CyFxSpiTransfer(offset, size, buf, CyFalse, CyTrue);

    if (NuandGetSPIManufacturer() != 0xEF) {
        status = NuandExso();
    }

    return status;
}

CyU3PReturnStatus_t NuandFlashInit() {
    CyU3PReturnStatus_t status;
    uint8_t mfn;

    NuandGPIOReconfigure(CyFalse, CyTrue);

    status = CyFxSpiInit();

    mfn = NuandGetSPIManufacturer();

    if (mfn == 0xEF || mfn == 0x1F) {
        CyU3PSpiSetClock(30000000);
    }

    if (mfn == 0x1F) {
        uint8_t location[1];

        location[0] = 0x06; /* Write Enable */
        CyU3PSpiSetSsnLine(CyFalse);
        CyU3PSpiTransmitWords(location, 1);
        CyU3PSpiSetSsnLine(CyTrue);

        location[0] = 0x98; /* Global Block Unlock */
        CyU3PSpiSetSsnLine(CyFalse);
        status = CyU3PSpiTransmitWords(location, 1);
        CyU3PSpiSetSsnLine(CyTrue);
    }

    glAppMode = MODE_FW_CONFIG;

    return status;
}

void NuandFlashDeinit() {
    CyFxSpiDeInit();
}

static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

int NuandExtractField(char *ptr, int len, char *field,
                            char *val, size_t  maxlen) {
    int c, wlen;
    unsigned char *ub, *end;
    unsigned short a1, a2;
    int flen;

    flen = strlen(field);

    ub = (unsigned char *)ptr;
    end = ub + len;
    while (ub < end) {
        c = *ub;

        if (c == 0xff) // flash and OTP are 0xff if they've never been written to
            break;

        a1 = *(unsigned short *)(&ub[c+1]);  // read checksum
        a2 = zcrc(ub, c+1);  // calculate checksum

        if (a1 == a2 || 1) {
            if (!strncmp((char *)ub + 1, field, flen)) {
                wlen = min_sz(c - flen, maxlen);
                strncpy(val, (char *)ub + 1 + flen, wlen);
                val[wlen] = 0;
                return 0;
            }
        } else {
            return 1;
        }
        ub += c + 3; //skip past `c' bytes, 2 byte CRC field, and 1 byte len field
    }
    return 1;
}
