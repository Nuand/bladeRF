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
#include <Windows.h>
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
#include "backend_config.h"
#include "backend/generic_usb.h"
#include "backend/generic_usb_types.h"
#include "conversions.h"
#include "minmax.h"
#include "log.h"
#include "flash.h"
#include <cyapi.h>

GUID guid = {0x35D5D3F1, 0x9D0E, 0x4F62, 0xBC, 0xFB, 0xB0, 0xD4, 0x8E,0xA6, 0x34, 0x16};

extern "C"
{
    extern generic_usb_fn bladerf_genusb_cypress_fn;
}

struct bladerf_Cypress_Data {
    CCyUSBDevice *CyDevice;
    CRITICAL_SECTION DeviceLock; 
};

#define MAKE_Device(x) CCyUSBDevice *USBDevice = ((bladerf_Cypress_Data *)(x))->CyDevice;
#define DeviceLock(x) EnterLock(&((bladerf_Cypress_Data *)(x))->DeviceLock);
#define DeviceUnlock(x) LeaveLock(&((bladerf_Cypress_Data *)(x))->DeviceLock);

void EnterLock(CRITICAL_SECTION* Lock)
{
    EnterCriticalSection(Lock); 
}

void LeaveLock(CRITICAL_SECTION* Lock)
{
    LeaveCriticalSection(Lock); 
}

CCyBulkEndPoint *GetEndPoint(CCyUSBDevice *USBDevice,int id)
{

    int eptCount ;
    eptCount = USBDevice->EndPointCount();

    for (int i=1; i<eptCount; i++) 
    {
        if (USBDevice->EndPoints[i]->Address == id)
            return (CCyBulkEndPoint *)USBDevice->EndPoints[i]; 
    }
    return NULL;
}


int cypress_bulk_transfer(void *DriverData, uint8_t endpoint, void*Buffer, uint32_t BufferLength ,uint32_t *BytesTransfered, uint32_t TimeOut)
{
    int Result = 0;
    DeviceLock(DriverData);

    MAKE_Device(DriverData);

        
    CCyBulkEndPoint *EP = GetEndPoint(USBDevice, endpoint);
    if (EP)
    {
        LONG len=BufferLength;
        USBDevice->ControlEndPt->TimeOut = TimeOut ? TimeOut : 0xffffffff;
        EP->LastError=0;
        EP->bytesWritten=0;
        if (EP->XferData((PUCHAR)Buffer,len))
        {
            *BytesTransfered=len;
            if (*BytesTransfered != BufferLength)
                Result = BLADERF_ERR_IO;
        }
        else
        {
            *BytesTransfered=len;
            Result = BLADERF_ERR_IO;
        }
    }
    else
    {
        Result = BLADERF_ERR_IO;
    }

    DeviceUnlock(DriverData);

    return Result;
}


int cypress_change_setting(void *DriverData, uint8_t setting)
{
    MAKE_Device(DriverData);
    if (USBDevice->SetAltIntfc(setting))
    {
        return 0;
    }
    else
        return BLADERF_ERR_IO;
}


static int cypress_control_transfer(void *DriverData,GEN_USB_TRANSFER_TARGET_TYPE Target, GEN_USB_TRANSFER_REQUEST_TYPE RequestType, GEN_USB_TRANSFER_DIRECTION Direction, uint8_t request, uint16_t wValue, uint16_t wIndex, void* Buffer, uint32_t BufferSize , uint32_t *BytesTransfered , uint32_t timeout )
{
    DeviceLock(DriverData);
    MAKE_Device(DriverData);

    int Result=0;
    
    switch(Direction)
    {
        case GEN_USB_TRANSFER_DIRECTION::DIRECTION_FROM_DEVICE:
            USBDevice->ControlEndPt->Direction = (CTL_XFER_DIR_TYPE::DIR_FROM_DEVICE);
            break;
        case GEN_USB_TRANSFER_DIRECTION::DIRECTION_TO_DEVICE:
            USBDevice->ControlEndPt->Direction = (CTL_XFER_DIR_TYPE::DIR_TO_DEVICE);
            break;
    }

    switch(RequestType)
    {
        case GEN_USB_TRANSFER_REQUEST_TYPE::REQUEST_CLASS:
            USBDevice->ControlEndPt->ReqType = CTL_XFER_REQ_TYPE::REQ_CLASS;
            break;
        case GEN_USB_TRANSFER_REQUEST_TYPE::REQUEST_STANDARD:
            USBDevice->ControlEndPt->ReqType = CTL_XFER_REQ_TYPE::REQ_STD;
            break;
        case GEN_USB_TRANSFER_REQUEST_TYPE::REQUEST_VENDOR:
            USBDevice->ControlEndPt->ReqType = CTL_XFER_REQ_TYPE::REQ_VENDOR;
            break;
    }

    switch (Target)
    {
        case GEN_USB_TRANSFER_TARGET_TYPE::TARGET_DEVICE:
            USBDevice->ControlEndPt->Target =  CTL_XFER_TGT_TYPE::TGT_DEVICE;
            break;
        case GEN_USB_TRANSFER_TARGET_TYPE::TARGET_ENDPOINT:
            USBDevice->ControlEndPt->Target =  CTL_XFER_TGT_TYPE::TGT_ENDPT;
            break;
        case GEN_USB_TRANSFER_TARGET_TYPE::TARGET_INTERFACE:
            USBDevice->ControlEndPt->Target =  CTL_XFER_TGT_TYPE::TGT_INTFC;
            break;
        case GEN_USB_TRANSFER_TARGET_TYPE::TARGET_OTHER:
            USBDevice->ControlEndPt->Target =  CTL_XFER_TGT_TYPE::TGT_OTHER;
            break;
    }

    USBDevice->ControlEndPt->MaxPktSize = BufferSize;
    USBDevice->ControlEndPt->ReqCode = request;
    USBDevice->ControlEndPt->Index = wIndex;
    USBDevice->ControlEndPt->Value = wValue;
    USBDevice->ControlEndPt->TimeOut = timeout ? timeout : 0xffffffff;
    LONG LEN = BufferSize;
    bool res = USBDevice->ControlEndPt->XferData((PUCHAR)Buffer,LEN);
    if (res)
    {
        if (BytesTransfered)
            *BytesTransfered = LEN;
    }
    else
    {
        if (BytesTransfered)
            *BytesTransfered = 0;
        Result = BLADERF_ERR_IO;
    }

    DeviceUnlock(DriverData);
    return Result;
}

static int cypress_open(struct bladerf **device, void** DriverData , struct bladerf_devinfo *info)
{
    int status=0;
    int Result=BLADERF_ERR_IO;

    CCyUSBDevice *USBDevice = new CCyUSBDevice(NULL, guid);

    if (USBDevice->Open(info->instance))
    {
        struct bladerf_Cypress_Data *DevData;
        DevData = (struct bladerf_Cypress_Data *)calloc(1,sizeof(struct bladerf_Cypress_Data));
        if (!InitializeCriticalSectionAndSpinCount(&DevData->DeviceLock, 0x00000400) ) 
        {
            delete USBDevice;
            free(DevData);
            return BLADERF_ERR_IO;
        }
        DevData->CyDevice = USBDevice;
        *DriverData= DevData;
        USBDevice->SetAltIntfc(1);
        return 0;
    }
    else
        delete USBDevice;

    return Result;
}

int genusb_cypress_probe(struct bladerf_devinfo_list *info_list)
{
    int status=0;
    CCyUSBDevice *USBDevice = new CCyUSBDevice(NULL, guid);
    for (int i=0; i< USBDevice->DeviceCount(); i++)
    {
        struct bladerf_devinfo info;
        if (USBDevice->Open(i))
        {
            info.backend = BLADERF_BACKEND_GENERIC_USB;
            info.instance = i;
            wcstombs(info.serial, USBDevice->SerialNumber, sizeof(info.serial));
            info.usb_addr = USBDevice->USBAddress;
            info.usb_bus = 0;
            info.backendParameter = &bladerf_genusb_cypress_fn;
            status = bladerf_devinfo_list_add(info_list, &info);
            if( status ) {
                log_error( "Could not add device to list: %s\n", bladerf_strerror(status) );
            } else {
                log_verbose("Added instance %d to device list\n",
                    info.instance);
            }
            USBDevice->Close();
        }
    }
    delete USBDevice;
    return status;
}

int cypress_get_speed(void *DriverData,bladerf_dev_speed *device_speed)
{
    MAKE_Device(DriverData);
    if (USBDevice->bHighSpeed)
    {
        *device_speed = BLADERF_DEVICE_SPEED_HIGH;
    }
    else
    {
        if (USBDevice->bSuperSpeed)
        {
            *device_speed = BLADERF_DEVICE_SPEED_SUPER;
        }
        else
        {
            *device_speed = BLADERF_DEVICE_SPEED_UNKNOWN;
        }
    }
    return 0;
}

int cypress_get_string_descriptor(void *DriverData, uint8_t Index, void* Buffer, int BufferSize)
{
    MAKE_Device(DriverData);
    uint32_t BytesTransfered=0;
    return cypress_control_transfer(DriverData, TARGET_DEVICE, REQUEST_STANDARD, DIRECTION_FROM_DEVICE, 0x06, 0x0300 | Index, 0, Buffer, BufferSize, &BytesTransfered, BLADE_USB_TIMEOUT_MS);
}

int cypress_close(void *DriverData) 
{
    struct bladerf_Cypress_Data *DevData = (struct bladerf_Cypress_Data *)DriverData;
    DevData->CyDevice->Close();
    DeleteCriticalSection(&(DevData->DeviceLock));
    free(DriverData);
    return 0;
}

struct cypressStreamData
{
	HANDLE *Handles;
	OVERLAPPED  *ov;
	PUCHAR *Token;
	CCyBulkEndPoint *EP81;
};

int cypress_stream_init(void *DriverData,struct bladerf_stream *stream)
{
    MAKE_Device(DriverData);

    cypressStreamData *bData = (cypressStreamData *)malloc(sizeof(cypressStreamData));
    stream->backend_data = bData;
    bData->Handles = (HANDLE*) calloc(1,stream->num_transfers * sizeof(HANDLE*));
    bData->ov = (OVERLAPPED*) calloc(1,stream->num_transfers * sizeof(OVERLAPPED));
    bData->Token = (PUCHAR*) calloc(1,stream->num_transfers * sizeof(PUCHAR*));
    bData->EP81 = GetEndPoint(USBDevice, 0x81);
    bData->EP81->XferMode = XFER_MODE_TYPE::XMODE_DIRECT;

    for (unsigned int i=0; i<stream->num_transfers; i++)
    {
        bData->Handles[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
        bData->ov[i].hEvent = bData->Handles[i];
    }

    return 0;
}

int cypress_stream(void *DriverData,struct bladerf_stream *stream, bladerf_module module)
{
    log_verbose( "Stream Start\n");
	cypressStreamData *bData = (cypressStreamData *) stream->backend_data;
	LONG buffer_size = (LONG)c16_samples_to_bytes(stream->samples_per_buffer);
	for (unsigned int i=0; i<stream->num_transfers; i++)
	{
		bData->Token[i] = bData->EP81->BeginDataXfer((PUCHAR)stream->buffers[i],buffer_size, &bData->ov[i]);
	}
	log_verbose( "Stream Setup\n");
	while(true)
	{
		DWORD res = WaitForMultipleObjects( (DWORD) stream->num_transfers, bData->Handles, false, INFINITE);
		if ((res >= WAIT_OBJECT_0) && (res <= WAIT_OBJECT_0+stream->num_transfers))
		{
			int idx = res - WAIT_OBJECT_0;
			long len =0;
			void* NextBuffer=NULL;
			log_verbose( "Got Buffer %d\n",idx);
			if (bData->EP81->FinishDataXfer((PUCHAR)stream->buffers[idx] , len, &bData->ov[idx], bData->Token[idx]))
			{ 
				bData->Token[idx] = NULL;
				NextBuffer = stream->cb(stream->dev, stream, NULL, stream->buffers[idx], len/4, stream->user_data);
				log_verbose( "Next Buffer %x\n",NextBuffer);
			}
			else
			{
				printf("Error res = %d\n",res);
				return 0;
			}
			if (NextBuffer != NULL)
				bData->Token[idx] = bData->EP81->BeginDataXfer((PUCHAR)NextBuffer,buffer_size, &bData->ov[idx]);
			else
				break;
		}
		else
		{
			printf("Error res = %d\n",res);
			return 0;
		}
	}
	stream->state = STREAM_SHUTTING_DOWN;
	log_verbose( "Teardown \n");
    bData->EP81->Abort();
	for (unsigned int i=0; i<stream->num_transfers; i++)
	{
		LONG len=0;
		if (bData->Token[i] != NULL)
		{
			bData->EP81->FinishDataXfer((PUCHAR)stream->buffers[i] , len, &bData->ov[i], bData->Token[i]);
		}
	}
	stream->state = STREAM_DONE;
	log_verbose("Stream complete \n");
    return 0;

}

int cypress_deinit_stream(void *DriverData,struct bladerf_stream *stream)
{
    return 0;
}


extern "C"
{
    generic_usb_fn bladerf_genusb_cypress_fn = {
        FIELD_INIT(.probe, genusb_cypress_probe),
        FIELD_INIT(.open, cypress_open),
        FIELD_INIT(.get_speed, cypress_get_speed),
        FIELD_INIT(.control_transfer, cypress_control_transfer),
        FIELD_INIT(.change_setting, cypress_change_setting),
        FIELD_INIT(.bulk_transfer, cypress_bulk_transfer),
        FIELD_INIT(.get_string_descriptor, cypress_get_string_descriptor),
        FIELD_INIT(.close, cypress_close),
        FIELD_INIT(.stream_init, cypress_stream_init),
        FIELD_INIT(.stream, cypress_stream),
        FIELD_INIT(.deinit_stream, cypress_deinit_stream),
    };
}