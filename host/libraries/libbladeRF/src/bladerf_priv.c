#include <assert.h>
#include <string.h>
#include <libbladeRF.h>
#include <stddef.h>

#include "bladerf_priv.h"
#include "log.h"

#define OTP_BUFFER_SIZE 256
#define CAL_BUFFER_SIZE 256

void bladerf_set_error(struct bladerf_error *error,
                        bladerf_error type, int val)
{
    error->type = type;
    error->value = val;
}

void bladerf_get_error(struct bladerf_error *error,
                        bladerf_error *type, int *val)
{
    if (type) {
        *type = error->type;
    }

    if (val) {
        *val = error->value;
    }
}

/* TODO Check for truncation (e.g., odd # bytes)? */
size_t bytes_to_c16_samples(size_t n_bytes)
{
    return n_bytes / (2 * sizeof(int16_t));
}

/* TODO Overflow check? */
size_t c16_samples_to_bytes(size_t n_samples)
{
    return n_samples * 2 * sizeof(int16_t);
}

int bladerf_init_device(struct bladerf *dev)
{
    unsigned int actual ;

    /* Set the GPIO pins to enable the LMS and select the low band */
    bladerf_config_gpio_write( dev, 0x57 );

    /* Set the internal LMS register to enable RX and TX */
    bladerf_lms_write( dev, 0x05, 0x3e );

    /* LMS FAQ: Improve TX spurious emission performance */
    bladerf_lms_write( dev, 0x47, 0x40 );

    /* LMS FAQ: Improve ADC performance */
    bladerf_lms_write( dev, 0x59, 0x29 );

    /* LMS FAQ: Common mode voltage for ADC */
    bladerf_lms_write( dev, 0x64, 0x36 );

    /* LMS FAQ: Higher LNA Gain */
    bladerf_lms_write( dev, 0x79, 0x37 );

    /* FPGA workaround: Set IQ polarity for RX */
    bladerf_lms_write( dev, 0x5a, 0xa0 );

    /* Set a default saplerate */
    bladerf_set_sample_rate( dev, BLADERF_MODULE_TX, 1000000, &actual );
    bladerf_set_sample_rate( dev, BLADERF_MODULE_RX, 1000000, &actual );

    /* Enable TX and RX */
    bladerf_enable_module( dev, BLADERF_MODULE_TX, false );
    bladerf_enable_module( dev, BLADERF_MODULE_RX, false );

    /* Set a default frequency of 1GHz */
    bladerf_set_frequency( dev, BLADERF_MODULE_TX, 1000000000 );
    bladerf_set_frequency( dev, BLADERF_MODULE_RX, 1000000000 );

    /* Set the calibrated VCTCXO DAC value */
    bladerf_dac_write( dev, dev->dac_trim );

    /* TODO: Read this return from the SPI calls */
    return 0;
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

static int extract_field(char *ptr, int len, char *field,
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

        if (a1 == a2) {
            if (!strncmp((char *)ub + 1, field, flen)) {
                wlen = min_sz(c - flen, maxlen);
                strncpy(val, (char *)ub + 1 + flen, wlen);
                val[wlen] = 0;
                return 0;
            }
        } else {
            log_warning( "%s: Field checksum mismatch\n", __FUNCTION__);
            return BLADERF_ERR_INVAL;
        }
        ub += c + 3; //skip past `c' bytes, 2 byte CRC field, and 1 byte len field
    }
    return BLADERF_ERR_INVAL;
}


int bladerf_get_otp_field(struct bladerf *dev, char *field,
                             char *data, size_t data_size)
{
    int status;
    char otp[OTP_BUFFER_SIZE];

    status = dev->fn->get_otp(dev, otp);
    if (status < 0)
        return status;
    else
        return extract_field(otp, OTP_BUFFER_SIZE, field, data, data_size);
}

int bladerf_get_cal_field(struct bladerf *dev, char *field,
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

int bladerf_get_and_cache_serial(struct bladerf *dev)
{
    int status;
    status = bladerf_get_otp_field(dev, "S", dev->serial,
                                    BLADERF_SERIAL_LENGTH - 1);

    if (status < 0) {
        log_debug("Unable to fetch serial number. Defaulting to 0's\n");
        memset(dev->serial, 0, BLADERF_SERIAL_LENGTH);
    }

    return status;
}

int bladerf_get_and_cache_vctcxo_trim(struct bladerf *dev)
{
    int status;
    bool ok;
    int16_t trim;
    char tmp[7] = { 0 };

    status = bladerf_get_cal_field(dev, "DAC", tmp, sizeof(tmp) - 1);
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

int bladerf_get_and_cache_fpga_size(struct bladerf *device)
{
    int status;
    char tmp[7] = { 0 };

    status = bladerf_get_cal_field(device, "B", tmp, sizeof(tmp) - 1);

    if (!strcmp("40", tmp)) {
        device->fpga_size = BLADERF_FPGA_40KLE;
    } else if(!strcmp("115", tmp)) {
        device->fpga_size = BLADERF_FPGA_115KLE;
    } else {
        device->fpga_size = BLADERF_FPGA_UNKNOWN;
    }

    return status;
}
