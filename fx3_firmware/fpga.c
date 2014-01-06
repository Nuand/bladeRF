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
#include <cyu3error.h>
#include <cyu3gpio.h>
#include <cyu3usb.h>
#include <cyu3spi.h>
#include "bladeRF.h"
#include "gpif.h"
#include "spi_flash_lib.h"

/* DMA Channel for RF U2P (USB to P-port) transfers */
static CyU3PDmaChannel glChHandlebladeRFUtoP;

static uint32_t glDMARxCount = 0;                  /* Counter to track the number of buffers received
                                             * from USB during FPGA programming */
static uint16_t glFlipLut[256];

int FpgaBeginProgram(void)
{
    CyBool_t value;

    unsigned tEnd;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    apiRetStatus = CyU3PGpioSetValue(GPIO_nCONFIG, CyFalse);
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
    uint32_t i, tmpr, tmpw;

    for (i = 0; i < 256; i++) {
        tmpr = i;

        tmpw = 0;
#define US_GPIF_2_FPP(GPIFbit, FPPbit)      tmpw |= (tmpr & (1 << FPPbit)) ? (1 << GPIFbit) : 0;
        US_GPIF_2_FPP(7,  0);
        US_GPIF_2_FPP(15, 1);
        US_GPIF_2_FPP(6,  2);
        US_GPIF_2_FPP(2,  3);
        US_GPIF_2_FPP(1,  4);
        US_GPIF_2_FPP(3,  5);
        US_GPIF_2_FPP(9,  6);
        US_GPIF_2_FPP(11, 7);
        glFlipLut[i] = tmpw;
    }
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
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
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

    NuandAllowSuspend(CyFalse);

    NuandGPIOReconfigure(CyFalse, !first_call);
    first_call = 0;

    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_FPGA_LOAD);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "Failed to configure GPIF, Error Code = %d\n",
                        apiRetStatus);
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

        default:
            CyU3PDebugPrint(4, "Error! Invalid USB speed.\n");
            CyFxAppErrorHandler(CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    apiRetStatus = CyU3PSetEpConfig(BLADE_FPGA_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
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
        CyU3PDebugPrint(4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(BLADE_FPGA_EP_PRODUCER);

    /* Set DMA channel transfer size. */
    apiRetStatus = CyU3PDmaChannelSetXfer(&glChHandlebladeRFUtoP, BLADE_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
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
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_DISABLED);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "Failed to disable GPIF, Error Code = %d\n",
                        apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    NuandAllowSuspend(CyTrue);
    glAppMode = MODE_NO_CONFIG;
    CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
}

uint8_t FPGA_status_bits[] = {
    [BLADE_FPGA_EP_PRODUCER] = 0,
};

CyBool_t NuandFpgaConfigHaltEndpoint(CyBool_t set, uint16_t endpoint)
{
    CyBool_t isHandled = CyFalse;

    switch(endpoint) {
    case BLADE_FPGA_EP_PRODUCER:
        FPGA_status_bits[endpoint] = set;
        ClearDMAChannel(endpoint, &glChHandlebladeRFUtoP,
                        BLADE_DMA_TX_SIZE, set);

        isHandled = !set;
        break;
    }

    return isHandled;
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

CyBool_t NuandLoadFromFlash(int fpga_len)
{
    uint8_t *ptr;
    int nleft;
    CyBool_t retval = CyFalse;
    CyU3PDmaBuffer_t dbuf;
    uint32_t sector_idx = 1025;
    uint32_t prodCnt, consCnt;
    int32_t i;
    CyU3PDmaState_t state;

    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    NuandFpgaConfigStart();
    ptr = CyU3PDmaBufferAlloc(4096);
    apiRetStatus = CyFxSpiInit(0x100);

    CyFxSpiFastRead(CyTrue);
    apiRetStatus = CyU3PSpiSetClock(30000000);

    FpgaBeginProgram();
    nleft = fpga_len;

    while(nleft) {
        apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP, &state, &prodCnt, &consCnt);
        if (apiRetStatus)
            break;

        CyU3PDmaChannelAbort(&glChHandlebladeRFUtoP);

        apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP, &state, &prodCnt, &consCnt);
        if (apiRetStatus)
            break;

        CyU3PDmaChannelReset(&glChHandlebladeRFUtoP);

        apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP, &state, &prodCnt, &consCnt);
        if (apiRetStatus)
            break;

        if (CyFxSpiTransfer(sector_idx++, 0x100, ptr, CyTrue) != CY_U3P_SUCCESS)
            break;

        uint8_t *end_in_b = &( ((uint8_t *)ptr)[255]);
        uint16_t *end_in_w = &( ((uint16_t *)ptr)[255]);

        /* Flip the bits in such a way that the FPGA can be programmed
         * This mapping can be determined by looking at the schematic */
        for (i = 255; i >= 0; i--)
            *end_in_w-- = glFlipLut[*end_in_b--];

        dbuf.buffer = ptr;
        dbuf.count = ((nleft > 256) ? 256 : (nleft + 2)) * 2;
        dbuf.size = 4096;
        dbuf.status = 0;

        apiRetStatus = CyU3PDmaChannelSetupSendBuffer(&glChHandlebladeRFUtoP, &dbuf);
        if (apiRetStatus)
            break;

        apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP, &state, &prodCnt, &consCnt);
        if (apiRetStatus)
            break;

        apiRetStatus = CyU3PDmaChannelWaitForCompletion(&glChHandlebladeRFUtoP, 100);
        if (apiRetStatus)
            break;

        apiRetStatus = CyU3PDmaChannelGetStatus(&glChHandlebladeRFUtoP, &state, &prodCnt, &consCnt);
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

    CyU3PDmaBufferFree(ptr);
    CyU3PSpiDeInit();

    CyFxSpiFastRead(CyFalse);
    NuandFpgaConfigStop();

    return retval;
}

const struct NuandApplication NuandFpgaConfig = {
    .start = NuandFpgaConfigStart,
    .stop = NuandFpgaConfigStop,
    .halt_endpoint = NuandFpgaConfigHaltEndpoint,
    .halted = NuandFpgaConfigHalted,
};
