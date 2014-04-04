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
#include <cyu3uart.h>
#include "gpif.h"
#include "rf.h"

static CyU3PDmaChannel glChHandlebladeRFUtoUART;   /* DMA Channel for U2P transfers */
static CyU3PDmaChannel glChHandlebladeRFUARTtoU;   /* DMA Channel for U2P transfers */

static CyU3PDmaChannel glChHandleUtoP;
static CyU3PDmaChannel glChHandlePtoU;

static int loopback = 0;
static int loopback_when_created;

void NuandRFLinkLoopBack(int lp) {
    loopback = lp;
}

static void UartBridgeStart(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PUartConfig_t uartConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;


    /* Initialize the UART for printing debug messages */
    apiRetStatus = CyU3PUartInit();
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set UART configuration */
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_4M; // CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyTrue;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiRetStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set UART Tx and Rx transfer Size to infinite */
    apiRetStatus = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyFxAppErrorHandler(apiRetStatus);
    }

    apiRetStatus = CyU3PUartRxSetBlockXfer(0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS) {
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
            CyU3PDebugPrint (4, "Error! Invalid USB speed.\n");
            CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(BLADE_UART_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(BLADE_UART_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    CyU3PMemSet((uint8_t *)&dmaCfg, 0, sizeof(dmaCfg));
    dmaCfg.size  = 16;
    dmaCfg.count = 10;
    dmaCfg.prodSckId = CY_U3P_UIB_SOCKET_PROD_2;
    dmaCfg.consSckId = CY_U3P_LPP_SOCKET_UART_CONS;
    dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaCfg.notification = 0;
    dmaCfg.cb = 0;
    dmaCfg.prodHeader = 0;
    dmaCfg.prodFooter = 0;
    dmaCfg.consHeader = 0;
    dmaCfg.prodAvailCount = 0;

    apiRetStatus = CyU3PDmaChannelCreate(&glChHandlebladeRFUtoUART,
            CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    dmaCfg.prodSckId = CY_U3P_LPP_SOCKET_UART_PROD;
    dmaCfg.consSckId = CY_U3P_UIB_SOCKET_CONS_2;
    apiRetStatus = CyU3PDmaChannelCreate(&glChHandlebladeRFUARTtoU,
            CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(BLADE_UART_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_UART_EP_CONSUMER);

    /* Set DMA channel transfer size */
    apiRetStatus = CyU3PDmaChannelSetXfer(&glChHandlebladeRFUtoUART, BLADE_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    apiRetStatus = CyU3PDmaChannelSetXfer(&glChHandlebladeRFUARTtoU, BLADE_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

     /* Set UART Tx and Rx transfer Size to infinite */
     apiRetStatus = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);
     if (apiRetStatus != CY_U3P_SUCCESS) {
         CyFxAppErrorHandler(apiRetStatus);
     }

     apiRetStatus = CyU3PUartRxSetBlockXfer(0xFFFFFFFF);
     if (apiRetStatus != CY_U3P_SUCCESS) {
         CyFxAppErrorHandler(apiRetStatus);
     }
}

static void UartBridgeStop(void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(BLADE_UART_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_UART_EP_CONSUMER);

    /* Destroy the channel */
    CyU3PDmaChannelDestroy(&glChHandlebladeRFUARTtoU);
    CyU3PDmaChannelDestroy(&glChHandlebladeRFUtoUART);

    /* Disable endpoints. */
    CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Producer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(BLADE_UART_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    apiRetStatus = CyU3PSetEpConfig(BLADE_UART_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
    CyU3PUartDeInit();
}


/* This function starts the RF data transport mechanism. This is the second
 * interface of the first and only descriptor. */
static void NuandRFLinkStart(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PDmaMultiChannelConfig_t dmaMultiConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    NuandAllowSuspend(CyFalse);
    NuandGPIOReconfigure(CyTrue, CyTrue);

    CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
    CyU3PGpioSetValue(GPIO_RX_EN, CyFalse);
    CyU3PGpioSetValue(GPIO_TX_EN, CyFalse);
    CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_RF_LINK);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "Failed to configure GPIF, Error code = %d\n",
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
            CyU3PDebugPrint (4, "Error! Invalid USB speed.\n");
            CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED ? 15 : 1);
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(BLADE_RF_SAMPLE_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(BLADE_RF_SAMPLE_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    // multi variant
    dmaMultiConfig.size = size * 2;
    dmaMultiConfig.count = 22;
    dmaMultiConfig.validSckCount = 2;
    dmaMultiConfig.prodSckId[0] = BLADE_RF_SAMPLE_EP_PRODUCER_USB_SOCKET;
    dmaMultiConfig.consSckId[0] = CY_U3P_PIB_SOCKET_2;
    dmaMultiConfig.consSckId[1] = CY_U3P_PIB_SOCKET_3;
    dmaMultiConfig.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaMultiConfig.notification = 0;
    dmaMultiConfig.cb = 0;
    dmaMultiConfig.prodHeader = 0;
    dmaMultiConfig.prodFooter = 0;
    dmaMultiConfig.consHeader = 0;
    dmaMultiConfig.prodAvailCount = 0;

    // non multi variant
    CyU3PMemSet((uint8_t *)&dmaCfg, 0, sizeof(dmaCfg));
    dmaCfg.size  = size * 2;
    dmaCfg.count = 22;
    dmaCfg.prodSckId = BLADE_RF_SAMPLE_EP_PRODUCER_USB_SOCKET;
    dmaCfg.consSckId = CY_U3P_PIB_SOCKET_3;
    dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaCfg.notification = 0;
    dmaCfg.cb = 0;
    dmaCfg.prodHeader = 0;
    dmaCfg.prodFooter = 0;
    dmaCfg.consHeader = 0;
    dmaCfg.prodAvailCount = 0;

    loopback_when_created = loopback;

    if (loopback) {
        dmaCfg.prodSckId = BLADE_RF_SAMPLE_EP_PRODUCER_USB_SOCKET;
        dmaCfg.consSckId = BLADE_RF_SAMPLE_EP_CONSUMER_USB_SOCKET;
    }

    apiRetStatus = CyU3PDmaChannelCreate(&glChHandleUtoP, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);

    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    if (!loopback) {
        dmaCfg.prodSckId = CY_U3P_PIB_SOCKET_0;
        dmaCfg.consSckId = BLADE_RF_SAMPLE_EP_CONSUMER_USB_SOCKET;
        apiRetStatus = CyU3PDmaChannelCreate(&glChHandlePtoU, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
    }

    /* Flush the Endpoint memory */
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_CONSUMER);

    /* Set DMA channel transfer size. */

    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleUtoP, BLADE_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    if (!loopback) {
        apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandlePtoU, BLADE_DMA_TX_SIZE);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
    }

    UartBridgeStart();
    glAppMode = MODE_RF_CONFIG;

}

/* This function stops the slave FIFO loop application. This shall be called
 * whenever a RESET or DISCONNECT event is received from the USB host. The
 * endpoints are disabled and the DMA pipe is destroyed by this function. */
static void NuandRFLinkStop (void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
    CyU3PGpioSetValue(GPIO_RX_EN, CyFalse);
    CyU3PGpioSetValue(GPIO_TX_EN, CyFalse);

    /* Flush endpoint memory buffers */
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_CONSUMER);

    /* Destroy the channels */
    CyU3PDmaChannelDestroy(&glChHandleUtoP);
    if (!loopback_when_created)
        CyU3PDmaChannelDestroy(&glChHandlePtoU);

    /* Disable endpoints. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Disable producer endpoint */
    apiRetStatus = CyU3PSetEpConfig(BLADE_RF_SAMPLE_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Disable consumer endpoint */
    apiRetStatus = CyU3PSetEpConfig(BLADE_RF_SAMPLE_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Reset the GPIF */
    apiRetStatus = NuandConfigureGpif(GPIF_CONFIG_DISABLED);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "Failed to deinitialize GPIF. Error code = %d\n",
                        apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    UartBridgeStop();
    NuandAllowSuspend(CyTrue);
    glAppMode = MODE_NO_CONFIG;
}

static uint8_t RF_status_bits[] = {
    [BLADE_RF_SAMPLE_EP_PRODUCER] = 0,
    [BLADE_RF_SAMPLE_EP_CONSUMER] = 0,
    [BLADE_UART_EP_PRODUCER] = 0,
    [BLADE_UART_EP_CONSUMER] = 0,
};

CyBool_t NuandRFLinkHaltEndpoint(CyBool_t set, uint16_t endpoint)
{
    CyBool_t isHandled = CyFalse;

    switch(endpoint) {
    case BLADE_RF_SAMPLE_EP_PRODUCER:
    case BLADE_RF_SAMPLE_EP_CONSUMER:
    case BLADE_UART_EP_PRODUCER:
    case BLADE_UART_EP_CONSUMER:
        isHandled = !set;
        RF_status_bits[endpoint] = set;
        break;
    }

    switch(endpoint) {
    case BLADE_RF_SAMPLE_EP_PRODUCER:
        ClearDMAChannel(endpoint, &glChHandleUtoP, BLADE_DMA_TX_SIZE, set);
        break;
    case BLADE_RF_SAMPLE_EP_CONSUMER:
        ClearDMAChannel(endpoint, &glChHandlePtoU, BLADE_DMA_TX_SIZE, set);
        break;
    case BLADE_UART_EP_PRODUCER:
        ClearDMAChannel(endpoint,
                &glChHandlebladeRFUtoUART, BLADE_DMA_TX_SIZE, set);
        break;
    case BLADE_UART_EP_CONSUMER:
        ClearDMAChannel(endpoint,
                &glChHandlebladeRFUARTtoU, BLADE_DMA_TX_SIZE, set);
        break;
    }

    return isHandled;
}

CyBool_t NuandRFLinkHalted(uint16_t endpoint, uint8_t * data)
{
    CyBool_t isHandled = CyFalse;

    switch(endpoint) {
    case BLADE_RF_SAMPLE_EP_PRODUCER:
    case BLADE_RF_SAMPLE_EP_CONSUMER:
    case BLADE_UART_EP_PRODUCER:
    case BLADE_UART_EP_CONSUMER:
        *data = RF_status_bits[endpoint];
        isHandled = CyTrue;
        break;
    }

    return isHandled;
}

const struct NuandApplication NuandRFLink = {
    .start = NuandRFLinkStart,
    .stop = NuandRFLinkStop,
    .halt_endpoint = NuandRFLinkHaltEndpoint,
    .halted = NuandRFLinkHalted,
};
