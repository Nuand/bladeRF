#include <assert.h>
#include <string.h>
#include <libbladeRF.h>
#include <stddef.h>

#include "bladerf_priv.h"
#include "debug.h"

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
    bladerf_config_gpio_write( dev, 0x51 );

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
    bladerf_enable_module( dev, BLADERF_MODULE_TX, true );
    bladerf_enable_module( dev, BLADERF_MODULE_RX, true );

    /* Set a default frequency of 1GHz */
    bladerf_set_frequency( dev, BLADERF_MODULE_TX, 1000000000 );
    bladerf_set_frequency( dev, BLADERF_MODULE_RX, 1000000000 );

    /* TODO: Read this return from the SPI calls */
    return 0;
}

void bladerf_init_devinfo(struct bladerf_devinfo *d)
{
    d->backend  = BLADERF_BACKEND_ANY;
    strcpy(d->serial, DEVINFO_SERIAL_ANY);
    d->usb_bus  = DEVINFO_BUS_ANY;
    d->usb_addr = DEVINFO_ADDR_ANY;
    d->instance = DEVINFO_INST_ANY;
}

bool bladerf_devinfo_matches(struct bladerf_devinfo *a,
                             struct bladerf_devinfo *b)
{
    return
      bladerf_instance_matches(a,b) &&
      bladerf_serial_matches(a,b) &&
      bladerf_bus_addr_matches(a,b);
}

bool bladerf_instance_matches(struct bladerf_devinfo *a,
                              struct bladerf_devinfo *b)
{
    return a->instance == DEVINFO_INST_ANY ||
           b->instance == DEVINFO_INST_ANY ||
           a->instance == b->instance;
}

bool bladerf_serial_matches(struct bladerf_devinfo *a,
                            struct bladerf_devinfo *b)
{
    return !strcmp(a->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(b->serial, DEVINFO_SERIAL_ANY) ||
           !strcmp(a->serial, b->serial);
}

bool bladerf_bus_addr_matches(struct bladerf_devinfo *a,
                              struct bladerf_devinfo *b)
{
    bool bus_match, addr_match;

    bus_match = a->usb_bus == DEVINFO_BUS_ANY ||
                b->usb_bus == DEVINFO_BUS_ANY ||
                a->usb_bus == b->usb_bus;

    addr_match = a->usb_addr == DEVINFO_BUS_ANY ||
                 b->usb_addr == DEVINFO_BUS_ANY ||
                 a->usb_addr == b->usb_addr;

    return bus_match && addr_match;
}

int bladerf_devinfo_list_init(struct bladerf_devinfo_list *list)
{
    int status = 0;

    list->num_elt = 0;
    list->backing_size = 5;

    list->elt = malloc(list->backing_size * sizeof(struct bladerf_devinfo));

    if (!list->elt) {
        free(list);
        status = BLADERF_ERR_MEM;
    }

    return status;
}

int bladerf_devinfo_list_add(struct bladerf_devinfo_list *list,
                             struct bladerf_devinfo *info)
{
    int status = 0;
    struct bladerf_devinfo *info_tmp;

    if (list->num_elt >= list->backing_size) {
        info_tmp = realloc(list->elt, list->backing_size * 2);
        if (!info_tmp) {
            status = BLADERF_ERR_MEM;
        } else {
            list->elt = info_tmp;
        }
    }

    if (status == 0) {
        memcpy(&list->elt[list->num_elt], info, sizeof(*info));
        list->num_elt++;
    }

    return status;
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
            dbg_printf( "%s: Field checksum mistmatch\n", __FUNCTION__);
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
    return bladerf_get_otp_field(dev, "S", dev->serial,
                                    BLADERF_SERIAL_LENGTH - 1);
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
        if (ok) {
            dev->dac_trim = trim;
        } else {
            dbg_printf("DAC trim unprogrammed. Defaulting to 0x8000\n");
            dev->dac_trim = 0x8000;
        }
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
