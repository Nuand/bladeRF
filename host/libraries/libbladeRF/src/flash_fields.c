/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "bladerf_priv.h"
#include "flash_fields.h"
#include "flash.h"
#include "log.h"

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
    byte            *ptr= (byte *)data_p;

    while (count-- > 0) {
        crcval=(word)(crcval^(word)(((word)*ptr++)<<8));
        for (xx=0;xx<8;xx++) {
            if(crcval&0x8000) { crcval=(word)((word)(crcval<<1)^0x1021); }
            else              { crcval=(word)(crcval<<1);                }
        }
    }
    return(crcval);
}

static int extract_field(char *ptr, int len, char *field,
                            char *val, size_t  maxlen) {
    int c;
    unsigned char *ub, *end;
    unsigned short a1, a2;
    size_t flen, wlen;

    flen = strlen(field);

    ub = (unsigned char *)ptr;
    end = ub + len;
    while (ub < end) {
        c = *ub;

        if (c == 0xff) // flash and OTP are 0xff if they've never been written to
            break;

        a1 = LE16_TO_HOST(*(unsigned short *)(&ub[c+1]));  // read checksum
        a2 = crc16mp(0, ub, c+1);  // calculated checksum

        if (a1 == a2) {
            if (!strncmp((char *)ub + 1, field, flen)) {
                wlen = min_sz(c - flen, maxlen);
                strncpy(val, (char *)ub + 1 + flen, wlen);
                val[wlen] = 0;
                return 0;
            }
        } else {
            log_debug( "%s: Field checksum mismatch\n", __FUNCTION__);
            return BLADERF_ERR_INVAL;
        }
        ub += c + 3; //skip past `c' bytes, 2 byte CRC field, and 1 byte len field
    }
    return BLADERF_ERR_INVAL;
}

int encode_field(char *ptr, int len, int *idx,
                 const char *field,
                 const char *val)
{
    int vlen, flen, tlen;
    flen = (int)strlen(field);
    vlen = (int)strlen(val);
    tlen = flen + vlen + 1;

    if (tlen >= 256 || *idx + tlen >= len)
        return BLADERF_ERR_MEM;

    ptr[*idx] = flen + vlen;
    strcpy(&ptr[*idx + 1], field);
    strcpy(&ptr[*idx + 1 + flen], val);
    *(unsigned short *)(&ptr[*idx + tlen ]) = HOST_TO_LE16(crc16mp(0, &ptr[*idx ], tlen));
    *idx += tlen + 2;
    return 0;
}

int add_field(char *buf, int buf_len, const char *field_name, const char *val)
{
    int dummy_idx = 0;
    int i = 0;
    int rv;

    /* skip to the end, ignoring crc (don't want to further corrupt partially
     * corrupt data) */
    while(i < buf_len) {
        uint8_t field_len = buf[i];

        if(field_len == 0xff)
            break;

        /* skip past `field_len' bytes, 2 byte CRC field, and 1 byte len
         * field */
        i += field_len + 3;
    }


    rv = encode_field(buf + i, buf_len - i, &dummy_idx, field_name, val);
    if(rv < 0)
        return rv;

    return 0;
}

int get_otp_field(struct bladerf *dev, char *field,
                  char *data, size_t data_size)
{
    int status;
    char otp[OTP_BUFFER_SIZE];

    memset(otp, 0xff, OTP_BUFFER_SIZE);

    status = dev->fn->get_otp(dev, otp);
    if (status < 0)
        return status;
    else
        return extract_field(otp, OTP_BUFFER_SIZE, field, data, data_size);
}

int get_cal_field(struct bladerf *dev, char *field,
                  char *data, size_t data_size)
{
    int status;
    char cal[CAL_BUFFER_SIZE];

    status = dev->fn->get_cal(dev, cal);
    if (status < 0)
        return status;
    else
        return extract_field(cal, CAL_BUFFER_SIZE, field, data, data_size);
}

int read_serial(struct bladerf *dev, char *serial_buf)
{
    int status;

    status = get_otp_field(dev, "S", serial_buf, BLADERF_SERIAL_LENGTH - 1);

    if (status < 0) {
        log_info("Unable to fetch serial number. Defaulting to 0's.\n");
        memset(dev->ident.serial, '0', BLADERF_SERIAL_LENGTH - 1);

        /* Treat this as non-fatal */
        status = 0;
    }

    serial_buf[BLADERF_SERIAL_LENGTH - 1] = '\0';

    return status;
}

int get_and_cache_vctcxo_trim(struct bladerf *dev)
{
    int status;
    bool ok;
    int16_t trim;
    char tmp[7] = { 0 };

    status = get_cal_field(dev, "DAC", tmp, sizeof(tmp) - 1);
    if (!status) {
        trim = str2uint(tmp, 0, 0xffff, &ok);
    }

    if (!status && ok) {
        dev->dac_trim = trim;
    } else {
        log_debug("Unable to fetch DAC trim. Defaulting to 0x8000\n");
        dev->dac_trim = 0x8000;
    }

    return status;
}

int get_and_cache_fpga_size(struct bladerf *device)
{
    int status;
    char tmp[7] = { 0 };

    status = get_cal_field(device, "B", tmp, sizeof(tmp) - 1);

    if (!strcmp("40", tmp)) {
        device->fpga_size = BLADERF_FPGA_40KLE;
    } else if(!strcmp("115", tmp)) {
        device->fpga_size = BLADERF_FPGA_115KLE;
    } else {
        device->fpga_size = BLADERF_FPGA_UNKNOWN;
    }

    return status;
}
