/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013-2015 Nuand LLC
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

/* This is a Windows-specific USB backend using Cypress's CyAPI, which utilizes
 * the CyUSB3.sys driver (with a CyUSB3.inf modified to include the bladeRF
 * VID/PID).
 */

extern "C" {
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "bladeRF.h"    /* Firmware interface */

#include "backend/backend.h"
#include "backend/usb/usb.h"
#include "async.h"
#include "log.h"
}

#include <CyAPI.h>


/* This GUID must match that in the modified CyUSB3.inf used with the bladeRF */
static const GUID driver_guid = {
    0x35D5D3F1, 0x9D0E, 0x4F62, 0xBC, 0xFB, 0xB0, 0xD4, 0x8E,0xA6, 0x34, 0x16
};

/* "Private data" for the CyAPI backend */
struct bladerf_cyapi {
    CCyUSBDevice *dev;
    HANDLE mutex;
};

struct transfer {
    OVERLAPPED event;           /* Transfer completion event handle */
    PUCHAR handle;              /* Handle for in-flight transfer */
    PUCHAR buffer;              /* Buffer associated with transfer */
};

struct stream_data {
    CCyBulkEndPoint *ep;        /* Endpoint associated with the stream */

    struct transfer *transfers; /* Transfer slots */

    size_t num_transfers;       /* Max # of in-flight transfers */
    size_t num_avail;
    size_t avail_i;             /* Index of next available transfer slot */
    size_t inflight_i;          /* Index of in-flight transfer that is
                                 * expected to finish next */
};

static inline struct bladerf_cyapi * get_backend_data(void *driver)
{
    assert(driver);
    return (struct bladerf_cyapi *) driver;
}

static inline CCyUSBDevice * get_device(void *driver)
{
    struct bladerf_cyapi *backend_data = get_backend_data(driver);
    assert(backend_data->dev);
    return backend_data->dev;
}

static inline struct stream_data * get_stream_data(struct bladerf_stream *s)
{
    assert(s && s->backend_data);
    return (struct stream_data *) s->backend_data;
}

static int open_device(CCyUSBDevice *dev, int instance, HANDLE *mutex)
{
    int status = 0;
    bool success;
    DWORD last_error;
    const char prefix[] = "Global\\bladeRF-";
    const size_t mutex_name_len = strlen(prefix) + BLADERF_SERIAL_LENGTH;
    char *mutex_name = (char *) calloc(mutex_name_len, 1);

    if (mutex_name == NULL) {
        return BLADERF_ERR_MEM;
    } else {
        strcpy(mutex_name, prefix);
    }

    success = dev->Open(instance);
    if (!success) {
        status = BLADERF_ERR_IO;
        goto out;
    }

    wcstombs(mutex_name + strlen(prefix), dev->SerialNumber, sizeof(mutex_name) - BLADERF_SERIAL_LENGTH - 1);
    log_verbose("Mutex name: %s\n", mutex_name);

    SetLastError(0);
    *mutex = CreateMutex(NULL, true, mutex_name);
    last_error = GetLastError();
    if (mutex == NULL || last_error != 0) {
        log_debug("Unable to create device mutex: mutex=%p, last_error=%ld\n",
                  mutex, last_error);
        dev->Close();
        status = BLADERF_ERR_NODEV;
    }

out:
    free(mutex_name);
    return status;
}

static bool device_matches_target(CCyUSBDevice *dev,
                                  backend_probe_target target)
{
    bool matches = false;

    switch (target) {
        case BACKEND_PROBE_BLADERF:
            matches = (dev->VendorID == USB_NUAND_VENDOR_ID) &&
                      (dev->ProductID == USB_NUAND_BLADERF_PRODUCT_ID);
            break;

        case BACKEND_PROBE_FX3_BOOTLOADER:
            matches = (dev->VendorID == USB_CYPRESS_VENDOR_ID) &&
                      (dev->ProductID == USB_FX3_PRODUCT_ID);

            if (!matches) {
                matches = (dev->VendorID == USB_NUAND_VENDOR_ID) &&
                          (dev->ProductID == USB_NUAND_BLADERF_BOOT_PRODUCT_ID);
            }
            break;
    }

    return matches;
}

static int cyapi_probe(backend_probe_target probe_target,
                       struct bladerf_devinfo_list *info_list)
{
    CCyUSBDevice *dev = new CCyUSBDevice(NULL, driver_guid);
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

    for (int i = 0; i < dev->DeviceCount(); i++) {
        struct bladerf_devinfo info;
        bool opened;
        int status;

        opened = dev->Open(i);
        if (opened) {
            if (device_matches_target(dev, probe_target)) {
                const size_t max_serial = sizeof(info.serial) - 1;
                info.instance = i;
                memset(info.serial, 0, sizeof(info.serial));
                wcstombs(info.serial, dev->SerialNumber, max_serial);
                info.usb_addr = dev->USBAddress;
                info.usb_bus = 0; /* CyAPI doesn't provide this */
                info.backend = BLADERF_BACKEND_CYPRESS;
                status = bladerf_devinfo_list_add(info_list, &info);
                if (status != 0) {
                    log_error("Could not add device to list: %s\n",
                              bladerf_strerror(status));
                } else {
                    log_verbose("Added instance %d to device list\n",
                        info.instance);
                }
            }

            dev->Close();
        }
    }

    delete dev;
    return 0;
}

static int open_via_info(void **driver, backend_probe_target probe_target,
                         struct bladerf_devinfo *info_in,
                         struct bladerf_devinfo *info_out)
{
    int status;
    int instance = -1;
    struct bladerf_devinfo_list info_list;
    CCyUSBDevice *dev;
    struct bladerf_cyapi *cyapi_data;

    dev = new CCyUSBDevice(NULL, driver_guid);
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

    cyapi_data = (struct bladerf_cyapi *) calloc(1, sizeof(cyapi_data[0]));
    if (cyapi_data == NULL) {
        delete dev;
        return BLADERF_ERR_MEM;
    }

    bladerf_devinfo_list_init(&info_list);
    status = cyapi_probe(probe_target, &info_list);

    for (unsigned int i = 0; i < info_list.num_elt && status == 0; i++) {


        if (bladerf_devinfo_matches(&info_list.elt[i], info_in)) {
            instance = info_list.elt[i].instance;

            status = open_device(dev, instance, &cyapi_data->mutex);
            if (status == 0) {
                cyapi_data->dev = dev;
                *driver = cyapi_data;
                if (info_out != NULL) {
                    memcpy(info_out,
                           &info_list.elt[instance],
                           sizeof(info_out[0]));
                }
                status = 0;
                break;
            }
        }
    }

    if (status == 0 && instance < 0) {
        status = BLADERF_ERR_NODEV;
    }

    if (status != 0) {
        delete dev;
        CloseHandle(cyapi_data->mutex);
    }

    free(info_list.elt);
    return status;
}


static int cyapi_open(void **driver,
                     struct bladerf_devinfo *info_in,
                     struct bladerf_devinfo *info_out)
{
    return open_via_info(driver, BACKEND_PROBE_BLADERF, info_in, info_out);
}

static int cyapi_change_setting(void *driver, uint8_t setting)
{
    CCyUSBDevice *dev = get_device(driver);

    if (dev->SetAltIntfc(setting)) {
        return 0;
    } else {
        return BLADERF_ERR_IO;
    }
}


static void cyapi_close(void *driver)
{
    struct bladerf_cyapi *cyapi_data = get_backend_data(driver);
    cyapi_data->dev->Close();
    delete cyapi_data->dev;
    CloseHandle(cyapi_data->mutex);
    free(driver);

}

static int cyapi_get_speed(void *driver,
                          bladerf_dev_speed *device_speed)
{
    int status = 0;
    CCyUSBDevice *dev = get_device(driver);

    if (dev->bHighSpeed) {
        *device_speed = BLADERF_DEVICE_SPEED_HIGH;
    } else if (dev->bSuperSpeed) {
        *device_speed = BLADERF_DEVICE_SPEED_SUPER;
    } else {
        *device_speed = BLADERF_DEVICE_SPEED_UNKNOWN;
        status = BLADERF_ERR_UNEXPECTED;
        log_debug("%s: Unable to determine device speed.\n", __FUNCTION__);
    }

    return status;
}

static int cyapi_control_transfer(void *driver,
                                 usb_target target_type, usb_request req_type,
                                 usb_direction dir, uint8_t request,
                                 uint16_t wvalue, uint16_t windex,
                                 void *buffer, uint32_t buffer_len,
                                 uint32_t timeout_ms)
{
    CCyUSBDevice *dev = get_device(driver);
    LONG len;
    bool success;

    int status = 0;

    switch (dir) {
        case USB_DIR_DEVICE_TO_HOST:
            dev->ControlEndPt->Direction = DIR_FROM_DEVICE;
            break;
        case USB_DIR_HOST_TO_DEVICE:
            dev->ControlEndPt->Direction = DIR_TO_DEVICE;
            break;
    }

    switch (req_type) {
        case USB_REQUEST_CLASS:
            dev->ControlEndPt->ReqType = REQ_CLASS;
            break;
        case USB_REQUEST_STANDARD:
            dev->ControlEndPt->ReqType = REQ_STD;
            break;
        case USB_REQUEST_VENDOR:
            dev->ControlEndPt->ReqType = REQ_VENDOR;
            break;
    }

    switch (target_type) {
        case USB_TARGET_DEVICE:
            dev->ControlEndPt->Target = TGT_DEVICE;
            break;
        case USB_TARGET_ENDPOINT:
            dev->ControlEndPt->Target = TGT_ENDPT;
            break;
        case USB_TARGET_INTERFACE:
            dev->ControlEndPt->Target = TGT_INTFC;
            break;
        case USB_TARGET_OTHER:
            dev->ControlEndPt->Target = TGT_OTHER;
            break;
    }

    dev->ControlEndPt->MaxPktSize = buffer_len;
    dev->ControlEndPt->ReqCode = request;
    dev->ControlEndPt->Index = windex;
    dev->ControlEndPt->Value = wvalue;
    dev->ControlEndPt->TimeOut = timeout_ms ? timeout_ms : INFINITE;
    len = buffer_len;

    success = dev->ControlEndPt->XferData((PUCHAR)buffer, len);
    if (!success) {
        status = BLADERF_ERR_IO;
    }

    return status;

}

static CCyBulkEndPoint *get_ep(CCyUSBDevice *USBDevice, int id)
{

    int count;
    count = USBDevice->EndPointCount();

    for (int i = 1; i < count; i++) {
        if (USBDevice->EndPoints[i]->Address == id) {
            return (CCyBulkEndPoint *) USBDevice->EndPoints[i];
        }
    }

    return NULL;
}

static int cyapi_bulk_transfer(void *driver, uint8_t endpoint, void *buffer,
                              uint32_t buffer_len, uint32_t timeout_ms)
{
    bool success;
    int status = BLADERF_ERR_IO;
    CCyUSBDevice *dev = get_device(driver);
    CCyBulkEndPoint *ep = get_ep(dev, endpoint);

    if (ep != NULL) {
        LONG len = buffer_len;
        dev->ControlEndPt->TimeOut = timeout_ms ? timeout_ms : INFINITE;
        success = ep->XferData((PUCHAR)buffer, len);

        if (success) {
            if (len == buffer_len) {
                status = 0;
            } else {
                log_debug("Transfer len mismatch: %u vs %u\n",
                          (unsigned int) buffer_len, (unsigned int) len);
            }
        } else {
            log_debug("Transfered failed.\n");
        }
    } else {
        log_debug("Failed to get EP handle.\n");
    }

    return status;
}

static int cyapi_get_string_descriptor(void *driver, uint8_t index,
                                      void *buffer, uint32_t buffer_len)
{
    int status;
    char *str;

    memset(buffer, 0, buffer_len);

    status = cyapi_control_transfer(driver,
                                    USB_TARGET_DEVICE,
                                    USB_REQUEST_STANDARD,
                                    USB_DIR_DEVICE_TO_HOST,
                                    0x06, 0x0300 | index, 0,
                                    buffer, buffer_len,
                                    BLADE_USB_TIMEOUT_MS);

    if (status != 0) {
        return status;
    }

    str = (char*)buffer;
    for (unsigned int i = 0; i < (buffer_len / 2); i++) {
        str[i] = str[2 + (i * 2)];
    }

    return 0;
}

static int cyapi_deinit_stream(void *driver, struct bladerf_stream *stream)
{
    struct stream_data *data;

    assert(stream != NULL);
    data =  get_stream_data(stream);
    assert(data != NULL);

    for (unsigned int i = 0; i < data->num_transfers; i++) {
        CloseHandle(data->transfers[i].event.hEvent);
    }

    free(data->transfers);
    free(data);

    return 0;
}

static int cyapi_init_stream(void *driver, struct bladerf_stream *stream,
                            size_t num_transfers)
{
    int status = BLADERF_ERR_MEM;
    CCyUSBDevice *dev = get_device(driver);
    struct stream_data *data;

    data = (struct stream_data *) calloc(1, sizeof(data[0]));
    if (data != NULL) {
        stream->backend_data = data;
    } else {
        return status;
    }

    data->transfers =
        (struct transfer*) calloc(num_transfers, sizeof(struct transfer));

    if (data->transfers == NULL) {
        goto out;
    }

    for (unsigned int i = 0; i < num_transfers; i++) {
        data->transfers[i].event.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (data->transfers[i].event.hEvent == NULL) {
            log_debug("%s: Failed to create EventObject for transfer %u\n",
                      (unsigned int) i);
            goto out;
        }
    }

   data->num_transfers = num_transfers;
   data->num_avail = num_transfers;
   data->inflight_i = 0;
   data->avail_i = 0;

   status = 0;

out:
    if (status != 0) {
        cyapi_deinit_stream(driver, stream);
        stream->backend_data = NULL;
    }

    return status;
}


#ifdef LOGGING_ENABLED
static inline int find_buf(void* ptr, struct bladerf_stream *stream)
{
    for (unsigned int i=0; i<stream->num_buffers; i++) {
        if (stream->buffers[i] == ptr) {
            return i;
        }
    }

    log_debug("Unabled to find buffer %p\n:", ptr);
    return -1;
}
#endif

#ifndef ENABLE_LIBBLADERF_ASYNC_LOG_VERBOSE
#undef log_verbose
#define log_verbose(...)
#endif

static inline size_t next_idx(struct stream_data *data, size_t i)
{
    return (i + 1) % data->num_transfers;
}

/* Assumes a transfer is available and the stream lock is being held */
static int submit_transfer(struct bladerf_stream *stream, void *buffer)
{
    int status = 0;
    PUCHAR xfer;
    struct stream_data *data = get_stream_data(stream);

    LONG buffer_size = (LONG) async_stream_buf_bytes(stream);

    assert(data->transfers[data->avail_i].handle == NULL);
    assert(data->transfers[data->avail_i].buffer == NULL);
    assert(data->num_avail != 0);

    xfer = data->ep->BeginDataXfer((PUCHAR) buffer, buffer_size,
                                   &data->transfers[data->avail_i].event);

    if (xfer != NULL) {
        data->transfers[data->avail_i].handle = xfer;
        data->transfers[data->avail_i].buffer = (PUCHAR) buffer;

        log_verbose("Submitted buffer %p using transfer slot %u.\n",
                    buffer, (unsigned int) data->avail_i);

        data->avail_i = next_idx(data, data->avail_i);
        data->num_avail--;
    } else {
        status = BLADERF_ERR_UNEXPECTED;
        log_debug("Failed to submit buffer %p in transfer slot %u.\n",
                  buffer, (unsigned int) data->avail_i);
    }

    return status;
}

static int cyapi_stream(void *driver, struct bladerf_stream *stream,
                       bladerf_module module)
{
    int status;
    int idx = 0;
    long len;
    void *next_buffer;
    ULONG timeout_ms;
    bool success, done;
    struct stream_data *data = get_stream_data(stream);
    struct bladerf_cyapi *cyapi = get_backend_data(driver);
    struct bladerf_metadata meta;

    assert(stream->dev->transfer_timeout[stream->module] <= ULONG_MAX);
    if (stream->dev->transfer_timeout[stream->module] == 0) {
        timeout_ms = INFINITE;
    } else {
        timeout_ms = stream->dev->transfer_timeout[stream->module];
    }

    switch (module) {
        case BLADERF_MODULE_RX:
            data->ep = get_ep(cyapi->dev, SAMPLE_EP_IN);
            break;

        case BLADERF_MODULE_TX:
            data->ep = get_ep(cyapi->dev, SAMPLE_EP_OUT);
            break;

        default:
            assert(!"Invalid module");
            return BLADERF_ERR_UNEXPECTED;
    }

    if (data->ep == NULL) {
        log_debug("Failed to get EP handle.\n");
        return BLADERF_ERR_UNEXPECTED;
    }

    data->ep->XferMode = XMODE_DIRECT;
    data->ep->Abort();
    data->ep->Reset();

    log_verbose("Starting stream...\n");
    status = 0;
    done = false;
    memset(&meta, 0, sizeof(meta));

    MUTEX_LOCK(&stream->lock);

    for (unsigned int i = 0; i < data->num_transfers && status == 0; i++) {
        if (module == BLADERF_MODULE_TX) {
            next_buffer = stream->cb(stream->dev, stream, &meta, NULL,
                                     stream->samples_per_buffer,
                                     stream->user_data);

            if (next_buffer == BLADERF_STREAM_SHUTDOWN) {
                done = true;
                break;
            } else if (next_buffer == BLADERF_STREAM_NO_DATA) {
                continue;
            }
        } else {
            next_buffer = stream->buffers[i];
        }

        status = submit_transfer(stream, next_buffer);
    }

    MUTEX_UNLOCK(&stream->lock);
    if (status != 0) {
        goto out;
    }

    while (!done) {
        struct transfer *xfer;
        size_t i;

        i = data->inflight_i;
        xfer = &data->transfers[i];
        success = data->ep->WaitForXfer(&xfer->event, timeout_ms);

        if (!success) {
            status = BLADERF_ERR_TIMEOUT;
            log_debug("Steam timed out.\n");
            break;
        }

        len = 0;
        next_buffer = NULL;

        log_verbose("Got transfer complete in slot %u (buffer %p)\n",
                    i, data->transfers[i].buffer);

        MUTEX_LOCK(&stream->lock);
        success = data->ep->FinishDataXfer(data->transfers[i].buffer, len,
                                           &data->transfers[i].event,
                                           xfer->handle);

        if (success) {
            next_buffer = stream->cb(stream->dev, stream, &meta,
                                     data->transfers[i].buffer,
                                     bytes_to_samples(stream->format, len),
                                     stream->user_data);

        } else {
            done = true;
            status = BLADERF_ERR_IO;
            log_debug("Failed to finish transfer %u, buf=%p.\n",
                      (unsigned int)i, &data->transfers[i].buffer);
        }

        data->transfers[i].buffer = NULL;
        data->transfers[i].handle = NULL;
        data->num_avail++;
        pthread_cond_signal(&stream->can_submit_buffer);

        if (next_buffer == BLADERF_STREAM_SHUTDOWN) {
            done = true;
        } else if (next_buffer != BLADERF_STREAM_NO_DATA) {
            status = submit_transfer(stream, next_buffer);
            done = (status != 0);
        }

        data->inflight_i = next_idx(data, data->inflight_i);
        MUTEX_UNLOCK(&stream->lock);
    }

out:

    MUTEX_LOCK(&stream->lock);
    stream->error_code = status;
    stream->state = STREAM_SHUTTING_DOWN;

    data->ep->Abort();
    data->ep->Reset();


    for (unsigned int i = 0; i < data->num_transfers; i++) {
        LONG len = 0;
        if (data->transfers[i].handle != NULL) {
            data->ep->FinishDataXfer(data->transfers[i].buffer, len,
                                     &data->transfers[i].event,
                                     data->transfers[i].handle);


            data->transfers[i].buffer = NULL;
            data->transfers[i].handle = NULL;
            data->num_avail++;
        }
    }

    assert(data->num_avail == data->num_transfers);

    stream->state = STREAM_DONE;
    log_verbose("Stream done (error_code = %d)\n", stream->error_code);
    MUTEX_UNLOCK(&stream->lock);

    return 0;
}
/* The top-level code will have acquired the stream->lock for us */
int cyapi_submit_stream_buffer(void *driver, struct bladerf_stream *stream,
                               void *buffer, unsigned int timeout_ms,
                               bool nonblock)
{
    int status = 0;
    struct timespec timeout_abs;
    struct stream_data *data = get_stream_data(stream);

    if (buffer == BLADERF_STREAM_SHUTDOWN) {
        if (data->num_avail == data->num_transfers) {
            stream->state = STREAM_DONE;
        } else {
            stream->state = STREAM_SHUTTING_DOWN;
        }

        return 0;
    }

    if (data->num_avail == 0) {
        if (nonblock) {
            log_debug("Non-blocking buffer submission requested, but no "
                      "transfers are currently available.");

            return BLADERF_ERR_WOULD_BLOCK;
        }

        if (timeout_ms != 0) {
            status = populate_abs_timeout(&timeout_abs, timeout_ms);
            if (status != 0) {
                return BLADERF_ERR_UNEXPECTED;
            }

            while (data->num_avail == 0 && status == 0) {
                status = pthread_cond_timedwait(&stream->can_submit_buffer,
                                                &stream->lock,
                                                &timeout_abs);
            }
        } else {
            while (data->num_avail == 0 && status == 0) {
                status = pthread_cond_wait(&stream->can_submit_buffer,
                                           &stream->lock);
            }
        }
    }

    if (status == ETIMEDOUT) {
        return BLADERF_ERR_TIMEOUT;
    } else if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        return submit_transfer(stream, buffer);
    }
}

int cyapi_open_bootloader(void **driver, uint8_t bus, uint8_t addr)
{
    struct bladerf_devinfo info;

    bladerf_init_devinfo(&info);
    info.backend = BLADERF_BACKEND_CYPRESS;
    info.usb_bus = bus;
    info.usb_addr = addr;

    return open_via_info(driver, BACKEND_PROBE_FX3_BOOTLOADER, &info, NULL);
}

extern "C" {
    static const struct usb_fns cypress_fns = {
        FIELD_INIT(.probe, cyapi_probe),
        FIELD_INIT(.open, cyapi_open),
        FIELD_INIT(.close, cyapi_close),
        FIELD_INIT(.get_speed, cyapi_get_speed),
        FIELD_INIT(.change_setting, cyapi_change_setting),
        FIELD_INIT(.control_transfer, cyapi_control_transfer),
        FIELD_INIT(.bulk_transfer, cyapi_bulk_transfer),
        FIELD_INIT(.get_string_descriptor, cyapi_get_string_descriptor),
        FIELD_INIT(.init_stream, cyapi_init_stream),
        FIELD_INIT(.stream, cyapi_stream),
        FIELD_INIT(.submit_stream_buffer, cyapi_submit_stream_buffer),
        FIELD_INIT(.deinit_stream, cyapi_deinit_stream),
        FIELD_INIT(.open_bootloader, cyapi_open_bootloader),
        FIELD_INIT(.close_bootloader, cyapi_close),
    };

    struct usb_driver usb_driver_cypress = {
        FIELD_INIT(.id, BLADERF_BACKEND_CYPRESS),
        FIELD_INIT(.fn, &cypress_fns)
    };
}
