/*
 * Copyright (c) 2013-2017 Nuand LLC
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
#include <cyu3error.h>
#include <cyu3gpio.h>
#include <cyu3os.h>
#include <cyu3usb.h>
#include <cyu3spi.h>
#include <string.h>
#include "bladeRF.h"
#include "fpga.h"
#include "gpif.h"
#include "LzmaDec.h"
#include "spi_flash_lib.h"

#define THIS_FILE LOGGER_ID_FPGA_C
#define FPGA_LZMA_BLOCK_BYTES (60u * 1024u)
#define FPGA_LZMA_MAX_BLOCK_BYTES 65504u

/* DMA Channel for RF U2P (USB to P-port) transfers */
static CyU3PDmaChannel glChHandlebladeRFUtoP;

/* Counter to track the number of buffers received from USB during FPGA
 * programming */
static uint32_t glDMARxCount = 0;

static uint16_t glFlipLut[256];

/* Tracks the last FPGA programmer (SPI flash or USB host) */
static NuandFpgaConfigSource glFpgaConfigSrc = NUAND_FPGA_CONFIG_SOURCE_INVALID;

int FpgaBeginProgram(void)
{
    CyBool_t value;

    unsigned tEnd;
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = CyU3PGpioSetValue(GPIO_nCONFIG, CyFalse);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        return apiRetStatus;
    }

    tEnd = CyU3PGetTime() + 10;
    while (CyU3PGetTime() < tEnd);
    apiRetStatus = CyU3PGpioSetValue(GPIO_nCONFIG, CyTrue);

    tEnd = CyU3PGetTime() + 1000;
    do {
        apiRetStatus = CyU3PGpioGetValue(GPIO_nSTATUS, &value);
        if (CyU3PGetTime() > tEnd)
            return -1;
    } while (!value);

    return 0;
}

void NuandFpgaConfigSwInit(void) {
    NuandFpgaConfigSwFlipLut(glFlipLut);
}

/* DMA callback function to handle the produce events for U to P transfers. */
static void bladeRFConfigUtoPDmaCallback(CyU3PDmaChannel *chHandle, CyU3PDmaCbType_t type, CyU3PDmaCBInput_t *input)
{
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    if (type == CY_U3P_DMA_CB_PROD_EVENT) {
        int i;

        uint8_t *end_in_b = &( ((uint8_t *)input->buffer_p.buffer)[input->buffer_p.count - 1]);
        uint16_t *end_in_w = &( ((uint16_t *)input->buffer_p.buffer)[input->buffer_p.count - 1]);


        /* Flip the bits in such a way that the FPGA can be programmed
         * This mapping can be determined by looking at the schematic */
        for (i = input->buffer_p.count - 1; i >= 0; i--) {
            *end_in_w-- = glFlipLut[*end_in_b--];
        }
        status = CyU3PDmaChannelCommitBuffer (chHandle, input->buffer_p.count * 2, 0);
        if (status != CY_U3P_SUCCESS) {
            LOG_ERROR(status);
        }

        /* Increment the counter. */
        glDMARxCount++;
    }
}

static void NuandFpgaConfigStart(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();
    static int first_call = 1;
    bool doUsb = true;

    NuandSetFpgaConfigSource(NUAND_FPGA_CONFIG_SOURCE_INVALID);

    NuandAllowSuspend(CyFalse);

    NuandGPIOReconfigure(CyFalse, !first_call);
    first_call = 0;

    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_FPGA_LOAD);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        LOG_ERROR(apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Determine max packet size based on USB speed */
    switch (usbSpeed)
    {
        case CY_U3P_FULL_SPEED:
            size = 64;
            break;

        case CY_U3P_HIGH_SPEED:
            size = 512;
            break;

        case CY_U3P_SUPER_SPEED:
            size = 1024;
            break;

        case CY_U3P_NOT_CONNECTED:
            size = 1024;
            doUsb = false;
            break;

        default:
            LOG_ERROR(usbSpeed);
            CyFxAppErrorHandler(CY_U3P_ERROR_FAILURE);
            break;
    }

    if (doUsb) {
        CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
        epCfg.enable = CyTrue;
        epCfg.epType = CY_U3P_USB_EP_BULK;
        epCfg.burstLen = 1;
        epCfg.streams = 0;
        epCfg.pcktSize = size;

        apiRetStatus = CyU3PSetEpConfig(BLADE_FPGA_EP_PRODUCER, &epCfg);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            LOG_ERROR(apiRetStatus);
            CyFxAppErrorHandler (apiRetStatus);
        }
    }

    dmaCfg.size  = size * 4;
    dmaCfg.count = BLADE_DMA_BUF_COUNT;
    dmaCfg.prodSckId = BLADE_FPGA_CONFIG_SOCKET;
    dmaCfg.consSckId = CY_U3P_PIB_SOCKET_3;
    dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;

    /* Enable the callback for produce event, this is where the bits will get flipped */
    dmaCfg.notification = CY_U3P_DMA_CB_PROD_EVENT;

    dmaCfg.cb = bladeRFConfigUtoPDmaCallback;
    dmaCfg.prodHeader = 0;
    dmaCfg.prodFooter = size * 3;
    dmaCfg.consHeader = 0;
    dmaCfg.prodAvailCount = 0;

    apiRetStatus = CyU3PDmaChannelCreate(&glChHandlebladeRFUtoP,
            CY_U3P_DMA_TYPE_MANUAL, &dmaCfg);

    if (apiRetStatus != CY_U3P_SUCCESS) {
        LOG_ERROR(apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    if (doUsb) {
        /* Flush the endpoint memory */
        CyU3PUsbFlushEp(BLADE_FPGA_EP_PRODUCER);
    }

    /* Set DMA channel transfer size. */
    apiRetStatus = CyU3PDmaChannelSetXfer(&glChHandlebladeRFUtoP, BLADE_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        LOG_ERROR(apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    glAppMode = MODE_FPGA_CONFIG;
}

void NuandFpgaConfigStop(void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Abort and clear the channel */
    CyU3PDmaChannelReset(&glChHandlebladeRFUtoP);

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(BLADE_FPGA_EP_PRODUCER);

    /* Destroy the channel */
    CyU3PDmaChannelDestroy(&glChHandlebladeRFUtoP);

    /* Disable endpoints. */
    CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Producer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(BLADE_FPGA_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        LOG_ERROR(apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_DISABLED);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        LOG_ERROR(apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    NuandAllowSuspend(CyTrue);
    glAppMode = MODE_NO_CONFIG;
    CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
}

uint8_t FPGA_status_bits[] = {
    [BLADE_FPGA_EP_PRODUCER] = 0,
};

CyU3PReturnStatus_t NuandFpgaConfigResetEndpoint(uint8_t endpoint)
{
    CyU3PReturnStatus_t status = CY_U3P_ERROR_BAD_ARGUMENT;

    switch(endpoint) {
        case BLADE_FPGA_EP_PRODUCER:
            status = ClearDMAChannel(endpoint, &glChHandlebladeRFUtoP,
                                    BLADE_DMA_TX_SIZE);
            break;
    }

    return status;
}

CyBool_t NuandFpgaConfigHaltEndpoint(CyBool_t set, uint16_t endpoint)
{
    CyBool_t isHandled = CyFalse;
    CyU3PReturnStatus_t status = CY_U3P_ERROR_BAD_ARGUMENT;

    switch(endpoint) {
    case BLADE_FPGA_EP_PRODUCER:
        FPGA_status_bits[endpoint] = set;
        status = NuandFpgaConfigResetEndpoint(endpoint);
        isHandled = !set;
        break;
    }

    if (status == CY_U3P_SUCCESS) {
        CyU3PUsbStall (endpoint, CyFalse, CyTrue);
        if(!set) {
            CyU3PUsbAckSetup ();
        }
    }

    return isHandled && status == CY_U3P_SUCCESS;
}

CyBool_t NuandFpgaConfigHalted(uint16_t endpoint, uint8_t * data)
{
    CyBool_t isHandled = CyFalse;

    switch(endpoint) {
    case BLADE_FPGA_EP_PRODUCER:
        *data = FPGA_status_bits[endpoint];
        isHandled = CyTrue;
        break;
    }

    return isHandled;
}

/**
 * Send one 256-byte FPGA configuration page through the GPIF loader.
 */
static CyU3PReturnStatus_t FpgaSendFlashBytes(uint8_t *ptr,
                                              int count,
                                              CyBool_t last)
{
    CyU3PDmaBuffer_t dbuf;
    CyU3PDmaState_t state;
    CyU3PReturnStatus_t apiRetStatus;
    uint32_t prodCnt, consCnt;
    int32_t i;
    uint8_t *end_in_b = &((uint8_t *)ptr)[255];
    uint16_t *end_in_w = &((uint16_t *)ptr)[255];

    apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP,
                                            &state, &prodCnt, &consCnt);
    if (apiRetStatus)
        return apiRetStatus;

    CyU3PDmaChannelAbort(&glChHandlebladeRFUtoP);

    apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP,
                                            &state, &prodCnt, &consCnt);
    if (apiRetStatus)
        return apiRetStatus;

    CyU3PDmaChannelReset(&glChHandlebladeRFUtoP);

    apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP,
                                            &state, &prodCnt, &consCnt);
    if (apiRetStatus)
        return apiRetStatus;

    for (i = 255; i >= 0; i--)
        *end_in_w-- = glFlipLut[*end_in_b--];

    dbuf.buffer = ptr;
    dbuf.count = ((last) ? count + 2 : count) * 2;
    dbuf.size = 4096;
    dbuf.status = 0;

    apiRetStatus = CyU3PDmaChannelSetupSendBuffer(&glChHandlebladeRFUtoP,
                                                  &dbuf);
    if (apiRetStatus)
        return apiRetStatus;

    apiRetStatus = CyU3PDmaChannelWaitForCompletion(&glChHandlebladeRFUtoP,
                                                    100);
    if (apiRetStatus)
        return apiRetStatus;

    return CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP,
                                    &state, &prodCnt, &consCnt);
}

CyBool_t NuandLoadFromFlash(int fpga_len)
{
    uint8_t *ptr;
    int nleft;
    CyBool_t retval = CyFalse;
    uint32_t sector_idx = 1025;

    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    NuandFpgaConfigStart();
    ptr = CyU3PDmaBufferAlloc(4096);
    apiRetStatus = CyFxSpiInit(0x100);

    CyFxSpiFastRead(CyTrue);
    apiRetStatus = CyU3PSpiSetClock(30000000);

    if (FpgaBeginProgram() != CY_U3P_SUCCESS) {
        goto out;
    }

    nleft = fpga_len;

    while(nleft) {
        if (CyFxSpiTransfer(sector_idx++, 0x100, ptr, CyTrue, CyFalse) != CY_U3P_SUCCESS)
            break;

        apiRetStatus = FpgaSendFlashBytes(ptr,
                (nleft > 256) ? 256 : nleft, nleft <= 256);
        if (apiRetStatus)
            break;

        if (nleft > 256) {
            nleft -= 256;
        } else {
            retval = CyTrue;
            nleft = 0;
            break;
        }
    }

    NuandSetFpgaConfigSource(NUAND_FPGA_CONFIG_SOURCE_FLASH);

out:
    CyU3PDmaBufferFree(ptr);
    CyU3PSpiDeInit();

    CyFxSpiFastRead(CyFalse);
    NuandFpgaConfigStop();

    return retval;
}

/**
 * Read a little-endian 32-bit value.
 */
static uint32_t load_le32(uint8_t *in)
{
    return (uint32_t)in[0] |
           ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) |
           ((uint32_t)in[3] << 24);
}

/**
 * Read a byte range from the compressed FPGA autoload stream.
 */
static CyU3PReturnStatus_t FpgaReadCompressedBytes(uint8_t *page,
                                                   uint32_t *sector_idx,
                                                   int *nleft,
                                                   int *offset,
                                                   int *avail,
                                                   uint8_t *out,
                                                   int count)
{
    int copied;

    for (copied = 0; copied < count;) {
        int take;

        if (*avail == 0) {
            if (*nleft <= 0 ||
                CyFxSpiTransfer((*sector_idx)++, 0x100, page,
                                CyTrue, CyFalse) != CY_U3P_SUCCESS) {
                return CY_U3P_ERROR_FAILURE;
            }

            *avail = (*nleft > 256) ? 256 : *nleft;
            *nleft -= *avail;
            *offset = 0;
        }

        take = (count - copied < *avail) ? count - copied : *avail;
        memcpy(out + copied, page + *offset, take);
        copied += take;
        *offset += take;
        *avail -= take;
    }

    return CY_U3P_SUCCESS;
}

/**
 * Send one decompressed block through the FPGA loader.
 */
static CyU3PReturnStatus_t FpgaSendDecodedBlock(uint8_t *page,
                                                uint8_t *block,
                                                int block_len,
                                                int *nleft)
{
    int offset;

    for (offset = 0; offset < block_len;) {
        CyU3PReturnStatus_t status;
        int count = (block_len - offset > 256) ? 256 : block_len - offset;

        CyU3PMemSet(page, 0xff, 256);
        memcpy(page, block + offset, count);
        *nleft -= count;

        status = FpgaSendFlashBytes(page, count, *nleft == 0);
        if (status != CY_U3P_SUCCESS) {
            return status;
        }

        offset += count;
    }

    return CY_U3P_SUCCESS;
}

/**
 * Allocate memory for the LZMA SDK.
 */
static void *FpgaLzmaAlloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return CyU3PMemAlloc((uint32_t)size);
}

/**
 * Free memory allocated by the LZMA SDK.
 */
static void FpgaLzmaFree(ISzAllocPtr p, void *addr)
{
    (void)p;
    if (addr != NULL) {
        CyU3PMemFree(addr);
    }
}

static const ISzAlloc fpga_lzma_allocator = {
    FpgaLzmaAlloc,
    FpgaLzmaFree
};

/**
 * Decode one LZMA frame into a caller-provided FPGA block buffer.
 */
static CyBool_t FpgaDecodeLzmaBlock(uint8_t *in,
                                    uint32_t clen,
                                    uint8_t *out,
                                    int block_len)
{
    ELzmaStatus status;
    SizeT src_len;
    SizeT dst_len;
    SRes lzma_status;

    if (clen <= LZMA_PROPS_SIZE) {
        return CyFalse;
    }

    src_len = (SizeT)(clen - LZMA_PROPS_SIZE);
    dst_len = (SizeT)block_len;
    lzma_status = LzmaDecode(out, &dst_len, in + LZMA_PROPS_SIZE,
                             &src_len, in, LZMA_PROPS_SIZE, LZMA_FINISH_END,
                             &status, &fpga_lzma_allocator);

    return lzma_status == SZ_OK &&
           dst_len == (SizeT)block_len &&
           src_len == (SizeT)(clen - LZMA_PROPS_SIZE);
}

/**
 * Load a compressed FPGA bitstream from SPI flash.
 */
CyBool_t NuandLoadCompressedFromFlash(int fpga_len, int compressed_len)
{
    uint8_t *in = NULL;
    uint8_t *out = NULL;
    uint8_t *page = NULL;
    uint8_t header[4];
    int in_avail = 0;
    int in_offset = 0;
    int nleft = fpga_len;
    int cleft = compressed_len;
    int decoded;
    CyBool_t retval = CyFalse;
    uint32_t sector_idx = 1025;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    if (fpga_len <= 0 || compressed_len <= 0) {
        return CyFalse;
    }

    NuandFpgaConfigStart();
    in = CyU3PDmaBufferAlloc((uint16_t)FPGA_LZMA_MAX_BLOCK_BYTES);
    out = CyU3PDmaBufferAlloc((uint16_t)FPGA_LZMA_BLOCK_BYTES);
    page = CyU3PDmaBufferAlloc(4096);
    if (in == NULL || out == NULL || page == NULL) {
        goto out;
    }

    apiRetStatus = CyFxSpiInit(0x100);

    CyFxSpiFastRead(CyTrue);
    apiRetStatus = CyU3PSpiSetClock(30000000);

    if (FpgaBeginProgram() != CY_U3P_SUCCESS) {
        goto out;
    }

    for (decoded = 0; decoded < fpga_len;) {
        const int block_len = (fpga_len - decoded > (int)FPGA_LZMA_BLOCK_BYTES) ?
                              (int)FPGA_LZMA_BLOCK_BYTES : fpga_len - decoded;
        uint32_t clen;

        apiRetStatus = FpgaReadCompressedBytes(page, &sector_idx, &cleft,
                                               &in_offset, &in_avail,
                                               header, sizeof(header));
        if (apiRetStatus != CY_U3P_SUCCESS) {
            goto out;
        }

        clen = load_le32(header);
        if (clen == 0 || clen > FPGA_LZMA_MAX_BLOCK_BYTES ||
            clen > (uint32_t)(cleft + in_avail)) {
            goto out;
        }

        apiRetStatus = FpgaReadCompressedBytes(page, &sector_idx, &cleft,
                                               &in_offset, &in_avail,
                                               in, (int)clen);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            goto out;
        }

        if (!FpgaDecodeLzmaBlock(in, clen, out, block_len)) {
            goto out;
        }

        apiRetStatus = FpgaSendDecodedBlock(in, out, block_len, &nleft);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            goto out;
        }

        decoded += block_len;
    }

    retval = (cleft == 0 && in_avail == 0 && nleft == 0) ? CyTrue : CyFalse;
    NuandSetFpgaConfigSource(NUAND_FPGA_CONFIG_SOURCE_FLASH);

out:
    if (in != NULL) {
        CyU3PDmaBufferFree(in);
    }
    if (out != NULL) {
        CyU3PDmaBufferFree(out);
    }
    if (page != NULL) {
        CyU3PDmaBufferFree(page);
    }
    CyU3PSpiDeInit();

    CyFxSpiFastRead(CyFalse);
    NuandFpgaConfigStop();

    return retval;
}

NuandFpgaConfigSource NuandGetFpgaConfigSource(void)
{
    return glFpgaConfigSrc;
}

void NuandSetFpgaConfigSource(NuandFpgaConfigSource src)
{
    glFpgaConfigSrc = src;
}

const struct NuandApplication NuandFpgaConfig = {
    .start = NuandFpgaConfigStart,
    .stop = NuandFpgaConfigStop,
    .halt_endpoint = NuandFpgaConfigHaltEndpoint,
    .halted = NuandFpgaConfigHalted,
    .reset_endpoint = NuandFpgaConfigResetEndpoint
};
