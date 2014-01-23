/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "rel_assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <bladeRF.h>

#include "bladerf_priv.h"
#include "backend/generic_usb.h"
#include "backend/generic_usb_types.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"
#include "flash.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define BLADERF_GENUSB_TIMEOUT_MS 1000

extern const generic_usb_fn bladerf_genusb_cypress_fn ;
extern const generic_usb_fn bladerf_genusb_libusb_fn ;

static const generic_usb_fn* DriverList[] = 
{
	&bladerf_genusb_cypress_fn,
	&bladerf_genusb_libusb_fn
};

struct bladerf_generic_usb {
    generic_usb_fn* Driver;
    void* DriverData;
};

const struct bladerf_fn bladerf_genusb_fn;

struct genusb_stream_data {
    int  completed;
};

#define MakeDriver(x) generic_usb_fn* Driver = (generic_usb_fn*) ((struct bladerf_generic_usb*) x->backend)->Driver; void* DriverData = ((struct bladerf_generic_usb*) x->backend)->DriverData;

/* Quick wrapper for vendor commands that get/send a 32-bit integer value */
static int vendor_command_int(struct bladerf *dev,
                          uint8_t cmd, GEN_USB_TRANSFER_DIRECTION dir, int32_t *val)
{
    uint32_t BytesOut=0;

    MakeDriver(dev);
    
    return Driver->control_transfer(DriverData, TARGET_INTERFACE, REQUEST_VENDOR, dir, cmd, 0,0, val, 4,&BytesOut  ,0);
}


/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wValue */
static int vendor_command_int_value(struct bladerf *dev,
                          uint8_t cmd, uint16_t wValue, int32_t *val)
{
    uint32_t BytesOut=0;

    MakeDriver(dev);
    
    return Driver->control_transfer(DriverData, TARGET_INTERFACE, REQUEST_VENDOR, DIRECTION_FROM_DEVICE, cmd, wValue,0, val, 4,&BytesOut  ,0);

}

/* Quick wrapper for vendor commands that get/send a 32-bit integer value + wIndex */
static int vendor_command_int_index(struct bladerf *dev,
                          uint8_t cmd, uint16_t wIndex, int32_t *val)
{
    uint32_t BytesOut=0;

    MakeDriver(dev);
    
    return Driver->control_transfer(DriverData, TARGET_INTERFACE, REQUEST_VENDOR, DIRECTION_FROM_DEVICE, cmd, 0,wIndex, val, 4,&BytesOut  ,0);
}

static int begin_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_BEGIN_PROG, DIRECTION_FROM_DEVICE, &result);

    if (status < 0) {
        return status;
    } else {
        return 0;
    }
}

static int end_fpga_programming(struct bladerf *dev)
{
    int result;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, DIRECTION_FROM_DEVICE, &result);
    return status;
}


static int genusb_is_fpga_configured(struct bladerf *dev)
{
    int result =-1;
    int status = vendor_command_int(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, DIRECTION_FROM_DEVICE, &result);

    if (status < 0) {
        return status;
    } else {
        return result;
    }
}


static int change_setting(struct bladerf *dev, uint8_t setting)
{
    MakeDriver(dev);
    return Driver->change_setting(DriverData,setting);
}

/* After performing a flash operation, switch back to either RF_LINK or the
 * FPGA loader.
 */
static int restore_post_flash_setting(struct bladerf *dev)
{
    int fpga_loaded = genusb_is_fpga_configured(dev);
    int status;

    if (fpga_loaded < 0) {
        log_debug("Not restoring alt interface setting - failed to check FPGA state\n");
        status = fpga_loaded;
    } else if (fpga_loaded) {
        status = change_setting(dev, USB_IF_RF_LINK);
    } else {
        /* Make sure we are using the configuration interface */
        if (dev->legacy & LEGACY_CONFIG_IF) {
            status = change_setting(dev, USB_IF_LEGACY_CONFIG);
        } else {
            status = change_setting(dev, USB_IF_CONFIG);
        }
    }

    return status;
}

int genusb_enable_module(struct bladerf *dev, bladerf_module m, bool enable) 
{
    int status;
    int32_t fx3_ret = -1;
    int ret_status = 0;
    uint16_t val;

    val = enable ? 1 : 0 ;

    if (dev->legacy) {
        int32_t val32 = val;
        status = vendor_command_int(dev, (m == BLADERF_MODULE_RX) ? BLADE_USB_CMD_RF_RX : BLADE_USB_CMD_RF_TX, DIRECTION_TO_DEVICE, &val32);
        fx3_ret = 0;
    } else {
        status = vendor_command_int_value(
                dev, (m == BLADERF_MODULE_RX) ? BLADE_USB_CMD_RF_RX : BLADE_USB_CMD_RF_TX,
                val, &fx3_ret);
    }
    if(status) {
        log_warning("Could not enable RF %s (%d): %s\n",
            (m == BLADERF_MODULE_RX) ? "RX" : "TX", status, bladerf_strerror(status) );
        ret_status = BLADERF_ERR_UNEXPECTED;
    } else if(fx3_ret) {
        log_warning("Error enabling RF %s (%d)\n",
                (m == BLADERF_MODULE_RX) ? "RX" : "TX", fx3_ret );
        ret_status = BLADERF_ERR_UNEXPECTED;
    }

    return ret_status;
}



/* FW < 1.5.0  does not have version strings */
static int genusb_fw_populate_version_legacy(struct bladerf *dev)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}

static int genusb_populate_fw_version(struct bladerf *dev)
{
    int status;
    uint16_t UniCodeBuffer[BLADERF_VERSION_STR_MAX];
    uint8_t AsciiBuffer[BLADERF_VERSION_STR_MAX];
    int i;
    MakeDriver(dev); 

    status = Driver->get_string_descriptor(DriverData,
                                                BLADE_USB_STR_INDEX_FW_VER,
                                                UniCodeBuffer,
                                                BLADERF_VERSION_STR_MAX*2);

    /* If we ran into an issue, we're likely dealing with an older firmware.
     * Fall back to the legacy version*/
    if (status != 0) {
        status = genusb_fw_populate_version_legacy(dev);
    } else 
    {
        for (i=0; i<BLADERF_VERSION_STR_MAX-1; i++)
        {
            AsciiBuffer[i]=(char)UniCodeBuffer[i+1];
        }
        memcpy((void*)dev->fw_version.describe, AsciiBuffer,BLADERF_VERSION_STR_MAX);
        status = str2version(dev->fw_version.describe, &dev->fw_version);
        if (status != 0) {
            status = BLADERF_ERR_UNEXPECTED;
        }
    }
    return status;
    return 0;
}

/* Returns BLADERF_ERR_* on failure */
static int access_peripheral(struct bladerf *dev, int per, int dir,
                                struct uart_cmd *cmd)
{
    uint8_t buf[16] = { 0 };    /* Zeroing out to avoid some valgrind noise
                                 * on the reserved items that aren't currently
                                 * used (i.e., bytes 4-15 */

    int status, driver_status;
    uint32_t transferred;
    MakeDriver(dev);

    /* Populate the buffer for transfer */
    buf[0] = UART_PKT_MAGIC;
    buf[1] = dir | per | 0x01;
    buf[2] = cmd->addr;
    buf[3] = cmd->data;

    /* Write down the command */
    driver_status = Driver->bulk_transfer(DriverData, 0x02, buf, 16, &transferred,BLADERF_GENUSB_TIMEOUT_MS);

    if (driver_status != 0) 
    {
        log_error("could not access peripheral\n");
        return BLADERF_ERR_IO;
    }

    /* If it's a read, we'll want to read back the result */
    transferred = 0;
    driver_status = status =  0;
    while (driver_status == 0 && transferred != 16) {
        driver_status = Driver->bulk_transfer(DriverData, 0x82, buf, 16,
                                             &transferred,
                                             BLADERF_GENUSB_TIMEOUT_MS);
    }

    if (driver_status != 0) {
        return BLADERF_ERR_IO;
    }

    /* Save off the result if it was a read */
    if (dir == UART_PKT_MODE_DIR_READ) {
        cmd->data = buf[3];
    }

    return status;

}

static int genusb_fpga_version_read(struct bladerf *dev, uint32_t *version) 
{
       int i = 0;
    int status = 0;
    struct uart_cmd cmd;
    *version = 0;

    for (i = 0; i < 4; i++){
        cmd.addr = i + UART_PKT_DEV_FGPA_VERSION_ID;
        cmd.data = 0xff;

        status = access_peripheral(
                                    dev,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_READ,
                                    &cmd
                                    );

        if (status < 0) {
            break;
        }

        *version |= (cmd.data << (i * 8));
    }

    if (status < 0){
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF,status);
    }

    return status;

}


static int genusb_populate_fpga_version(struct bladerf *dev)
{
    int status;
    uint32_t version;

    status = genusb_fpga_version_read(dev,&version);
    if (status < 0) {
        log_debug( "Could not retrieve FPGA version\n" ) ;
        dev->fpga_version.major = 0;
        dev->fpga_version.minor = 0;
        dev->fpga_version.patch = 0;
    }
    else {
        log_debug( "Raw FPGA Version: 0x%8.8x\n", version ) ;
        dev->fpga_version.major = (version >>  0) & 0xff;
        dev->fpga_version.minor = (version >>  8) & 0xff;
        dev->fpga_version.patch = (version >> 16) & 0xffff;
    }
    snprintf((char*)dev->fpga_version.describe, BLADERF_VERSION_STR_MAX,
                 "%d.%d.%d", dev->fpga_version.major, dev->fpga_version.minor,
                 dev->fpga_version.patch);

    return 0;
}

static int enable_rf(struct bladerf *dev) 
{
    int status;
    
    int ret_status = 0;

    status = change_setting(dev, USB_IF_RF_LINK);
    if(status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_debug( "alt_setting issue: %s\n", bladerf_strerror(status) );
        return status;
    }
    log_verbose( "Changed into RF link mode: %s\n", bladerf_strerror(status) ) ;

    /* We can only read the FPGA version once we've switched over to the
     * RF_LINK mode */
    ret_status = genusb_populate_fpga_version(dev);
    return ret_status;    
}

int genusb_probe(struct bladerf_devinfo_list *info_list)
{
    unsigned int i;
    const size_t n_drivers = ARRAY_SIZE(DriverList);

    for (i = 0; i < n_drivers; i++) 
    {
         DriverList[i]->probe(info_list);
    }
    return 0;
}

static int genusb_open(struct bladerf **device, struct bladerf_devinfo *info)
{
    int status=0;
    int Result = BLADERF_ERR_NODEV;
    unsigned int i;
    struct bladerf_devinfo_list info_list;
    struct bladerf *dev = NULL;

    status = bladerf_devinfo_list_init(&info_list);

    if (status != 0) {
        log_debug("Failed to initialize devinfo list: %s\n",
                  bladerf_strerror(status));
        return status;
    }

    status = genusb_probe(&info_list);

    if (status != 0) {
        log_debug("Failed to probe devices\n");
        return BLADERF_ERR_NODEV;
    }
    
    for (i=0; i < info_list.num_elt; i++)
    {
        if (bladerf_devinfo_matches(&info_list.elt[i],info))
        {
            
            struct bladerf_generic_usb *BackendData = NULL;  
            
            generic_usb_fn* Driver = (generic_usb_fn*) info_list.elt[i].backendParameter;
     
            dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
            BackendData = (struct bladerf_generic_usb *)calloc(1, sizeof(struct bladerf_generic_usb));
            BackendData->Driver = Driver;

            memcpy(&dev->ident, &info_list.elt[i], sizeof(struct bladerf_devinfo));
            dev->fn = &bladerf_genusb_fn;
            dev->backend = BackendData;
            dev->fpga_version.describe = (const char*)calloc(1, BLADERF_VERSION_STR_MAX + 1);
            dev->fw_version.describe = (const char*) calloc(1, BLADERF_VERSION_STR_MAX + 1);

            Result = Driver->open(device,&BackendData->DriverData,&info_list.elt[i]);
            if (Result != 0)
            {
                free(BackendData);
                free(dev);
            }
            else
            {
                *device = dev;
                status = genusb_populate_fw_version(dev);
                if (status != 0) {
                    log_debug("Failed to populate FW version (instance %d): %s\n",
                              bladerf_strerror(status));
                    break;
                }

            }
            break;
        }
    }

    if (dev) 
    {
        if (genusb_is_fpga_configured(dev)) 
        {
            enable_rf(dev);
        }
    }


    if (info_list.num_elt != 0)
        free(info_list.elt);
    return Result;
}

static int genusb_close(struct bladerf *dev)
{
    MakeDriver(dev);
    Driver->close(DriverData);
    return 0;
}

static int genusb_load_fpga(struct bladerf *dev, uint8_t *image, size_t image_size)
{
    unsigned int wait_count;
    int status = 0;
    uint32_t transferred = 0;
    MakeDriver(dev);

    /* Make sure we are using the configuration interface */
    if (dev->legacy & LEGACY_CONFIG_IF) {
        status = change_setting(dev, USB_IF_LEGACY_CONFIG);
    } else {
        status = change_setting(dev, USB_IF_CONFIG);
    }

    if(status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        log_debug( "alt_setting issue: %d\n", bladerf_strerror(status) );
        return status;;
    }

    /* Begin programming */
    status = begin_fpga_programming(dev);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Send the file down */
    assert(image_size <= INT_MAX);
    status = Driver->bulk_transfer(DriverData, 0x2, image, (uint32_t)image_size,&transferred, 0);
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* End programming */
    status = end_fpga_programming(dev);
    if (status) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    /* Poll FPGA status to determine if programming was a success */
    wait_count = 10;
    status = 0;

    while (wait_count > 0 && status == 0) {
        status = genusb_is_fpga_configured(dev);
        if (status == 1) {
            break;
        }

        usleep(200000);
        wait_count--;
    }

    /* Failed to determine if FPGA is loaded */
    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    } else if (wait_count == 0 && !status) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, BLADERF_ERR_TIMEOUT);
        return BLADERF_ERR_TIMEOUT;
    }

    status = enable_rf(dev);

    return status;

}

static int erase_sector(struct bladerf *dev, uint16_t sector)
{
  
    int status, erase_ret;
    uint32_t BytesRead;
    MakeDriver(dev);
     
    status = Driver->control_transfer(DriverData,
        TARGET_INTERFACE,
        REQUEST_VENDOR,
        DIRECTION_FROM_DEVICE,
        BLADE_USB_CMD_FLASH_ERASE,
        0,
        sector,
        (unsigned char *)&erase_ret,
        sizeof(erase_ret),
        &BytesRead,
        BLADERF_GENUSB_TIMEOUT_MS
    );

    if (BytesRead != sizeof(erase_ret)
        || ((!(dev->legacy & LEGACY_CONFIG_IF)) &&  erase_ret != 0))
    {
        log_error("Failed to erase sector at 0x%02x\n",
                flash_from_sectors(sector));

        if (status != 0)
            log_debug("status: %s\n", bladerf_strerror(status));
        else if (erase_ret != 0)
            log_debug("Received erase failure status from FX3. error = %d\n",
                      erase_ret);
        else
            log_debug("Unexpected read size: %d\n", status);

        return BLADERF_ERR_IO;
    }

    log_debug("Erased sector 0x%04x.\n", flash_from_sectors(sector));
    return 0;

}

static int genusb_erase_flash(struct bladerf *dev, uint32_t addr, uint32_t len)
{
 int sector_addr, sector_len;
    int i;
    int status = 0;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_SECTOR, addr, len))
        return BLADERF_ERR_MISALIGNED;

    sector_addr = flash_to_sectors(addr);
    sector_len  = flash_to_sectors(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_debug("Failed to set interface: %s\n", bladerf_strerror(status));
        return BLADERF_ERR_IO;
    }

    log_info("Erasing 0x%08x bytes starting at address 0x%08x.\n", len, addr);

    for (i=0; i < sector_len; i++) {
        status = erase_sector(dev, sector_addr + i);
        if(status)
            return status;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return len;
    }
}

static int read_buffer(struct bladerf *dev, uint8_t request,uint8_t *buf, uint16_t len)
{
    int status, read_size;
    int buf_off;

    MakeDriver(dev);

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        read_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        read_size = 64;
    } else {
        log_debug("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    /* only these two requests seem to use bytes in the control transfer
     * parameters instead of pages/sectors so watch out! */
    assert(request == BLADE_USB_CMD_READ_PAGE_BUFFER
           || request == BLADE_USB_CMD_READ_CAL_CACHE);

    assert(len % read_size == 0);

    for(buf_off = 0; buf_off < len; buf_off += read_size)
    {
        uint32_t Bytes_Read=0;
        status = Driver->control_transfer(DriverData, TARGET_DEVICE, REQUEST_VENDOR, DIRECTION_FROM_DEVICE,request, 0, buf_off,  &buf[buf_off],  read_size, &Bytes_Read,0);
        if(status != 0) 
        {
            log_debug("Failed to read page buffer at offset 0x%02x: %d\n",
                      buf_off, status);
            return status;
        } 
        else
        {
            if(Bytes_Read != read_size) 
            {
                log_debug("Got unexpected read size when writing page buffer at"
                          " offset 0x%02x: expected %d, got %d\n",
                          buf_off, read_size, status);

                return BLADERF_ERR_IO;
            }
        }
    }
    return 0;
}

static int read_page_buffer(struct bladerf *dev, uint8_t *buf)
{
 return read_buffer(
        dev,
        BLADE_USB_CMD_READ_PAGE_BUFFER,
        buf,
        BLADERF_FLASH_PAGE_SIZE
    );

  
}

static int genusb_get_cal(struct bladerf *dev, char *cal) 
{
    return read_buffer(
        dev,
        BLADE_USB_CMD_READ_CAL_CACHE,
        (uint8_t*)cal,
        CAL_BUFFER_SIZE
    );
}

static int legacy_read_one_page(struct bladerf *dev,
                                uint16_t page,
                                uint8_t *buf)
{
  int status = 0;
    int read_size;
     
    uint16_t buf_off;

    MakeDriver(dev);

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        read_size = 64;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        read_size = BLADERF_FLASH_PAGE_SIZE;
    } else {
        log_debug("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    for(buf_off=0; buf_off < BLADERF_FLASH_PAGE_SIZE; buf_off += read_size) 
    {
        uint32_t BytesRead = 0;
        status = Driver->control_transfer(DriverData,
            TARGET_INTERFACE,
            REQUEST_VENDOR,
            DIRECTION_FROM_DEVICE, 
            BLADE_USB_CMD_FLASH_READ,
            0,
            page,
            &buf[buf_off],
            read_size,
            &BytesRead,
            BLADERF_GENUSB_TIMEOUT_MS
        );

        if (BytesRead != read_size) {
            if (status != 0) {
                log_error("Failed to read back page at 0x%02x: %s\n",
                          flash_from_pages(page), bladerf_strerror(status));
            } else {
                log_error("Unexpected read size: %d\n", status);
            }
            return BLADERF_ERR_IO;
        }
    }

    return 0;

}

/* Assumes the device is already configured for USB_IF_SPI_FLASH */
static int read_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    int32_t read_status = -1;
    int status;

    if (dev->legacy & LEGACY_CONFIG_IF)
        return legacy_read_one_page(dev, page, buf);

    status = vendor_command_int_index(
        dev,
        BLADE_USB_CMD_FLASH_READ,
        page,
        &read_status
    );

    if(status != 0) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read page %d: %d\n", page, read_status);
        status = BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, (uint8_t*)buf);
}

static int genusb_read_flash(struct bladerf *dev, uint32_t addr,
                           uint8_t *buf, uint32_t len)
{
 int status;
    int page_addr, page_len, i;
    unsigned int read;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", bladerf_strerror(status));
        return BLADERF_ERR_IO;
    }

    log_info("Reading 0x%08x bytes from address 0x%08x.\n", len, addr);

    read = 0;
    for (i=0; i < page_len; i++) {
        log_debug("Reading page 0x%04x.\n", flash_from_pages(i));
        status = read_one_page(dev, page_addr + i, buf + read);
        if(status)
            return status;

        read += BLADERF_FLASH_PAGE_SIZE;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return read;
    }

}

static int compare_page_buffers(uint8_t *page_buf, uint8_t *image_page)
{
    int i;
    for (i = 0; i < BLADERF_FLASH_PAGE_SIZE; i++) {
        if (page_buf[i] != image_page[i]) {
            return -i;
        }
    }

    return 0;
}

static int verify_one_page(struct bladerf *dev,
                           uint16_t page, uint8_t *image_buf)
{
  int status = 0;
    uint8_t page_buf[BLADERF_FLASH_PAGE_SIZE];
    unsigned int i;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", bladerf_strerror(status));
        return BLADERF_ERR_IO;
    }

    log_debug("Verifying page 0x%04x.\n", flash_from_pages(page));
    status = read_one_page(dev, page, page_buf);
    if(status)
        return status;

    status = compare_page_buffers(page_buf, image_buf);
    if(status < 0) {
        i = abs(status);
        log_error("bladeRF firmware verification failed at flash "
                  " address 0x%02x. Read 0x%02X, expected 0x%02X\n",
                  flash_from_pages(page) + i,
                  page_buf[i],
                  image_buf[i]
            );

        return BLADERF_ERR_IO;
    }

    return restore_post_flash_setting(dev);

}

static int verify_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *image, uint32_t len)
{
    int status = 0;
    int page_addr, page_len, i;
    uint8_t *image_buf;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    log_info("Verifying 0x%08x bytes at address 0x%08x\n", len, addr);

    for(i=0; i < page_len; i++) {
        image_buf = &image[flash_from_pages(i)];
        status = verify_one_page(dev, page_addr + i, image_buf);
        if(status < 0)
            break;
    }

    return status;
}

static int write_buffer(struct bladerf *dev,
                        uint8_t request,
                        uint16_t page,
                        uint8_t *buf)
{
 
    int status;

    uint32_t buf_off;
    uint32_t write_size;
    
    MakeDriver(dev);

    if (dev->usb_speed == BLADERF_DEVICE_SPEED_SUPER) {
        write_size = BLADERF_FLASH_PAGE_SIZE;
    } else if (dev->usb_speed == BLADERF_DEVICE_SPEED_HIGH) {
        write_size = 64;
    } else {
        log_error("Encountered unknown USB speed in %s\n", __FUNCTION__);
        return BLADERF_ERR_UNEXPECTED;
    }

    for(buf_off = 0; buf_off < BLADERF_FLASH_PAGE_SIZE; buf_off += write_size)
    {
        uint32_t BytesOut = 0;
        status = Driver->control_transfer(
            DriverData,
            TARGET_INTERFACE,
            REQUEST_VENDOR,
            DIRECTION_TO_DEVICE,
            request,
            0,
            dev->legacy & LEGACY_CONFIG_IF ? page : buf_off,
            &buf[buf_off],
            write_size,
            &BytesOut,
            BLADERF_GENUSB_TIMEOUT_MS);

        if(status != 0) {
            log_error("Failed to write page buffer at offset 0x%02x"
                      " for page at 0x%02x: %s\n",
                      buf_off, flash_from_pages(page),
                      bladerf_strerror(status));

            return status;
        } else if((unsigned int)status != write_size) {
            log_error("Got unexpected write size when writing page buffer"
                      " at 0x%02x: expected %d, got %d\n",
                      flash_from_pages(page), write_size, status);

            return BLADERF_ERR_IO;
        }
    }

    return 0;

}

static int write_one_page(struct bladerf *dev, uint16_t page, uint8_t *buf)
{
    int status;
    int32_t write_status = -1;

    status = write_buffer(
        dev,
        dev->legacy & LEGACY_CONFIG_IF
            ? BLADE_USB_CMD_FLASH_WRITE : BLADE_USB_CMD_WRITE_PAGE_BUFFER,
        page,
        buf
    );
    if(status)
        return status;

    if(dev->legacy & LEGACY_CONFIG_IF)
        return 0;

    status =  vendor_command_int_index(
        dev,
        BLADE_USB_CMD_FLASH_WRITE,
        page,
        &write_status
    );

    if(status != 0) {
        return status;
    }

    if(write_status != 0) {
        log_error("Failed to write page at 0x%02x: %d\n",
                  flash_from_pages(page), write_status);

         return BLADERF_ERR_UNEXPECTED;
    }

#ifdef FLASH_VERIFY_PAGE_WRITES
    status = verify_one_page(dev, page, buf);
    if(status < 0)
        return status;
#endif

    return 0;
}

static int genusb_write_flash(struct bladerf *dev, uint32_t addr,
                        uint8_t *buf, uint32_t len)
{
 int status;
    int page_addr, page_len, written, i;

    if(!flash_bounds_aligned(BLADERF_FLASH_ALIGNMENT_PAGE, addr, len))
        return BLADERF_ERR_MISALIGNED;

    page_addr = flash_to_pages(addr);
    page_len  = flash_to_pages(len);

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", bladerf_strerror(status));
        return BLADERF_ERR_IO;
    }

    log_info("Writing 0x%08x bytes to address 0x%08x.\n", len, addr);

    written = 0;
    for(i=0; i < page_len; i++) {
        log_debug("Writing page at 0x%04x.\n", flash_from_pages(i));
        status = write_one_page(dev, page_addr + i, buf + written);
        if(status)
            return status;

        written += BLADERF_FLASH_PAGE_SIZE;
    }

    status = restore_post_flash_setting(dev);
    if (status != 0) {
        return status;
    } else {
        return written;
    }

}

static int genusb_flash_firmware(struct bladerf *dev,
                               uint8_t *image, size_t image_size)
{
    int status;

    assert(image_size <= UINT32_MAX);

    status = genusb_erase_flash(dev, 0, (uint32_t)image_size);

    if (status >= 0) {
        status = genusb_write_flash(dev, 0, image, (uint32_t)image_size);
    }

    if (status >= 0) {
        status = verify_flash(dev, 0, image, (uint32_t)image_size);
    }

    return status;
}

static int genusb_device_reset(struct bladerf *dev)
{
    MakeDriver(dev);
    Driver->control_transfer(DriverData, TARGET_INTERFACE, REQUEST_VENDOR, DIRECTION_TO_DEVICE, BLADE_USB_CMD_RESET,0,0,NULL,0, NULL,BLADERF_GENUSB_TIMEOUT_MS);
    return 0;
}

static int genusb_jump_to_bootloader(struct bladerf *dev)
{
    MakeDriver(dev);
    Driver->control_transfer(DriverData, TARGET_INTERFACE, REQUEST_VENDOR, DIRECTION_TO_DEVICE, BLADE_USB_CMD_JUMP_TO_BOOTLOADER,0,0,NULL,0, NULL,BLADERF_GENUSB_TIMEOUT_MS);
    return 0;
}

static int genusb_get_otp(struct bladerf *dev, char *otp)
{
     int status;
    int otp_page = 0;
    int32_t read_status = -1;

    status = change_setting(dev, USB_IF_SPI_FLASH);
    if (status) {
        log_error("Failed to set interface: %s\n", bladerf_strerror(status));
        return BLADERF_ERR_IO;
    }

    status = vendor_command_int_index(
            dev, BLADE_USB_CMD_READ_OTP,
            otp_page, &read_status);
    if(status != 0) {
        return status;
    }

    if(read_status != 0) {
        log_error("Failed to read OTP page %d: %d\n",
                otp_page, read_status);
        return BLADERF_ERR_UNEXPECTED;
    }

    return read_page_buffer(dev, (uint8_t*)otp);
}

static int genusb_get_device_speed(struct bladerf *dev,
                                 bladerf_dev_speed *device_speed)
{
    MakeDriver(dev);
    return Driver->get_speed(DriverData,device_speed);
}

static int genusb_config_gpio_write(struct bladerf *dev, uint32_t val)
{
   int i = 0;
    int status = 0;
    struct uart_cmd cmd;
   

    for (i = 0; status == 0 && i < 4; i++) {
        cmd.addr = i;
        cmd.data = (val>>(8*i))&0xff;
        status = access_peripheral(
                                    dev,
                                    UART_PKT_DEV_GPIO,
                                    UART_PKT_MODE_DIR_WRITE,
                                    &cmd
                                  );

        if (status < 0) {
            break;
        }
    }

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int genusb_config_gpio_read(struct bladerf *dev, uint32_t *val)
{
    int i = 0;
    int status = 0;
    struct uart_cmd cmd;
     

    *val = 0;
    for(i = UART_PKT_DEV_GPIO_ADDR; status == 0 && i < 4; i++) {
        cmd.addr = i;
        cmd.data = 0xff;
        status = access_peripheral(
                                    dev,
                                    UART_PKT_DEV_GPIO, UART_PKT_MODE_DIR_READ,
                                    &cmd
                                  );

        if (status != 0) {
            break;
        }

        *val |= (cmd.data << (8*i));
    }

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int genusb_si5338_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;
   
    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(dev, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int genusb_si5338_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status = 0;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_SI5338,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
    }

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );

    return status;
}

static int genusb_lms_write(struct bladerf *dev, uint8_t addr, uint8_t data)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = addr;
    cmd.data = data;

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, data );

    status = access_peripheral(dev, UART_PKT_DEV_LMS,
                                UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int genusb_lms_read(struct bladerf *dev, uint8_t addr, uint8_t *data)
{
    int status;
    struct uart_cmd cmd;
    

    cmd.addr = addr;
    cmd.data = 0xff;

    status = access_peripheral(dev, UART_PKT_DEV_LMS,
                               UART_PKT_MODE_DIR_READ, &cmd);

    if (status < 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    } else {
        *data = cmd.data;
    }

    log_verbose( "%s: 0x%2.2x 0x%2.2x\n", __FUNCTION__, addr, *data );

    return status;
}

static int genusb_dac_write(struct bladerf *dev, uint16_t value)
{
    int status;
    struct uart_cmd cmd;

    cmd.addr = 0 ;
    cmd.data = value & 0xff ;
    status = access_peripheral(dev, UART_PKT_DEV_VCTCXO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
        return status;
    }

    cmd.addr = 1 ;
    cmd.data = (value>>8)&0xff ;
    status = access_peripheral(dev, UART_PKT_DEV_VCTCXO,
                               UART_PKT_MODE_DIR_WRITE, &cmd);

    if (status != 0) {
        bladerf_set_error(&dev->error, ETYPE_LIBBLADERF, status);
    }

    return status;
}

static int set_fpga_correction(struct bladerf *dev, uint8_t addr, int16_t value)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}

static int genusb_set_correction(struct bladerf *dev, bladerf_correction_module module, int16_t value)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}


static int print_fpga_correction(struct bladerf *dev, uint8_t addr, int16_t *value)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}

static int genusb_print_correction(struct bladerf *dev, bladerf_correction_module module, int16_t *value)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}

static int genusb_tx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}

static int genusb_rx(struct bladerf *dev, bladerf_format format, void *samples,
                   int n, struct bladerf_metadata *metadata)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    return 0;
}


static int genusb_stream_init(struct bladerf_stream *stream)
{
    MakeDriver(stream->dev);
    return Driver->stream_init(DriverData,stream);
}

static int genusb_stream(struct bladerf_stream *stream, bladerf_module module)
{
    MakeDriver(stream->dev);
    return Driver->stream(DriverData,stream,module);
}

void genusb_deinit_stream(struct bladerf_stream *stream)
{
    MakeDriver(stream->dev);
    Driver->deinit_stream(DriverData,stream);
}

void genusb_set_transfer_timeout(struct bladerf *dev, bladerf_module module, int timeout) 
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
}

int genusb_get_transfer_timeout(struct bladerf *dev, bladerf_module module) {
  log_debug("NOT IMPLEMENTED %s",__FUNCTION__); 
  return 0;
}

static int genusb_get_stats(struct bladerf *dev, struct bladerf_stats *stats)
{
    log_debug("NOT IMPLEMENTED %s\n",__FUNCTION__); 
    /* TODO Implementation requires FPGA support */
    return BLADERF_ERR_UNSUPPORTED;
}

const struct bladerf_fn bladerf_genusb_fn = {
    FIELD_INIT(.probe, genusb_probe),

    FIELD_INIT(.open, genusb_open),
    FIELD_INIT(.close, genusb_close),

    FIELD_INIT(.load_fpga, genusb_load_fpga),
    FIELD_INIT(.is_fpga_configured, genusb_is_fpga_configured),

    FIELD_INIT(.flash_firmware, genusb_flash_firmware),

    FIELD_INIT(.erase_flash, genusb_erase_flash),
    FIELD_INIT(.read_flash, genusb_read_flash),
    FIELD_INIT(.write_flash, genusb_write_flash),
    FIELD_INIT(.device_reset, genusb_device_reset),
    FIELD_INIT(.jump_to_bootloader, genusb_jump_to_bootloader),

    FIELD_INIT(.get_cal, genusb_get_cal),
    FIELD_INIT(.get_otp, genusb_get_otp),
    FIELD_INIT(.get_device_speed, genusb_get_device_speed),

    FIELD_INIT(.config_gpio_write, genusb_config_gpio_write),
    FIELD_INIT(.config_gpio_read, genusb_config_gpio_read),

    FIELD_INIT(.set_correction, genusb_set_correction),
    FIELD_INIT(.print_correction, genusb_print_correction),

    FIELD_INIT(.si5338_write, genusb_si5338_write),
    FIELD_INIT(.si5338_read, genusb_si5338_read),

    FIELD_INIT(.lms_write, genusb_lms_write),
    FIELD_INIT(.lms_read, genusb_lms_read),

    FIELD_INIT(.dac_write, genusb_dac_write),

    FIELD_INIT(.enable_module, genusb_enable_module),
    FIELD_INIT(.rx, genusb_rx),
    FIELD_INIT(.tx, genusb_tx),

    FIELD_INIT(.init_stream, genusb_stream_init),
    FIELD_INIT(.stream, genusb_stream),
    FIELD_INIT(.deinit_stream, genusb_deinit_stream),

    FIELD_INIT(.set_transfer_timeout, genusb_set_transfer_timeout),
    FIELD_INIT(.get_transfer_timeout, genusb_get_transfer_timeout),

    FIELD_INIT(.stats, genusb_get_stats)
};
