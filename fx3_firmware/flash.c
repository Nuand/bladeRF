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

CyU3PReturnStatus_t NuandLockOtp() {
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

CyU3PReturnStatus_t NuandReadOtp(size_t offset, size_t size, void *buf) {
    CyU3PReturnStatus_t status;

    status = NuandEnso();
    status = CyFxSpiTransfer(offset, size, buf, CyTrue);
    status = NuandExso();

    return status;
}

CyU3PReturnStatus_t NuandWriteOtp(size_t offset, size_t size, void *buf) {
    CyU3PReturnStatus_t status;

    status = NuandEnso();
    status = CyFxSpiTransfer(offset, size, buf, CyFalse);
    status = NuandExso();

    return status;
}

void NuandFlashInit() {
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    NuandGPIOReconfigure(CyFalse, CyTrue);

    status = CyFxSpiInit();

    glAppMode = MODE_FW_CONFIG;
}

void NuandFlashDeinit() {
    CyFxSpiDeInit();
}

static inline size_t min_sz(size_t x, size_t y)
{
    return x < y ? x : y;
}

/******
 * CRC16 implementation from http://softwaremonkey.org/Code/CRC16
 */
typedef  unsigned char                   byte;    /*     8 bit unsigned       */
typedef  unsigned short int              word;    /*    16 bit unsigned       */

static word crc16mp(word crcval, void *data_p, word count) {
    /* CRC-16 Routine for processing multiple part data blocks.
     * Pass 0 into 'crcval' for first call for any given block; for
     * subsequent calls pass the CRC returned by the previous call. */
    word            xx;
    byte            *ptr=data_p;

    while (count-- > 0) {
        crcval=(word)(crcval^(word)(((word)*ptr++)<<8));
        for (xx=0;xx<8;xx++) {
            if(crcval&0x8000) { crcval=(word)((word)(crcval<<1)^0x1021); }
            else              { crcval=(word)(crcval<<1);                }
        }
    }
    return(crcval);
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
        a2 = crc16mp(0, ub, c+1);  // calculated checksum

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
