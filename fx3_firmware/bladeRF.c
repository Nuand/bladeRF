/*
 * bladeRF FX3 firmware (bladeRF.c)
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "bladeRF.h"
#include "cyu3gpif.h"
#include "cyu3pib.h"
#include "cyu3gpio.h"
#include "pib_regs.h"

/* This file should be included only once as it contains
 * structure definitions. Including it in multiple places
 * can result in linker error. */
#include "cyfxgpif_C4loader.h"
#include "cyfxgpif_RFlink.h"


uint32_t glAppMode = MODE_NO_CONFIG;

CyU3PThread bladeRFAppThread;

CyU3PDmaChannel glChHandlebladeRFUtoP;      /* DMA Channel for RF U2P (USB to P-port) transfers */
CyU3PDmaChannel glChHandlebladeRFPtoU;      /* DMA Channel for RF P2U (P-Port to USB) transfers */

CyU3PDmaChannel glChHandlebladeRFUtoUART;   /* DMA Channel for U2P transfers */
CyU3PDmaChannel glChHandlebladeRFUARTtoU;   /* DMA Channel for U2P transfers */

#ifdef TX_MULTI
CyU3PDmaMultiChannel glChHandleMultiUtoP;
#else
CyU3PDmaChannel glChHandleUtoP;
#endif

#ifdef RX_MULTI
CyU3PDmaMultiChannel glChHandleMultiPtoU;
#else
CyU3PDmaChannel glChHandlePtoU;
#endif

uint8_t glUsbConfiguration = 0;             /* Active USB configuration. */
uint8_t glUsbInterface = 0;                 /* Active USB interface. */

uint32_t glDMARxCount = 0;                  /* Counter to track the number of buffers received
                                             * from USB during FPGA programming */

uint8_t glEp0Buffer[4096] __attribute__ ((aligned (32)));
uint32_t glEp0Idx;

/* Application Error Handler */
void CyFxAppErrorHandler(CyU3PReturnStatus_t apiRetStatus)
{
    /* firmware failed with the error code apiRetStatus */

    /* Loop Indefinitely */
    for (;;)
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep(100);
}

void NuandGPIOReconfigure(CyBool_t fullGpif, CyBool_t warm)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    int i;

    struct {
        int pin;        // pin number
        int input;      // is this an input pin

        int warm;       // should this pin be enabled via IOMatrix() call?
                        // GPIF pins should be marked False, because they have to overriden
    } pins[] = {
        {GPIO_nSTATUS,  CyTrue,  CyTrue},
        {GPIO_CONFDONE, CyTrue,  CyTrue},
        {GPIO_SYS_RST,  CyFalse, CyFalse},
        {GPIO_RX_EN,    CyFalse, CyFalse},
        {GPIO_TX_EN,    CyFalse, CyFalse},
        {GPIO_nCONFIG,  CyFalse, CyTrue},
        {GPIO_ID,       CyTrue,  CyTrue},
        {GPIO_LED,      CyFalse,  CyTrue}
    };
#define ARR_SIZE(x) (sizeof(x)/sizeof(x[0]))

    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyFalse;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = !fullGpif;
    io_cfg.isDQ32Bit = fullGpif;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_DEFAULT;

    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = 0;
    if (warm) {
        for (i = 0; i < ARR_SIZE(pins); i++) {
            if (!pins[i].warm)
                continue;

            if (pins[i].pin < 32) {
                io_cfg.gpioSimpleEn[0] |= 1 << (pins[i].pin - 1);
            } else {
                io_cfg.gpioSimpleEn[1] |= 1 << (pins[i].pin - 32);
            }
        }
    }
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;

    status = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (status != CY_U3P_SUCCESS) {
        while(1);
    }

    for (i = 0; i < ARR_SIZE(pins); i++) {
        // the pin has already been activated by the call to IOMatrix()
        if (warm && pins[i].warm)
            continue;

        apiRetStatus = CyU3PDeviceGpioOverride(pins[i].pin, CyTrue);
        if (apiRetStatus != 0) {
            CyU3PDebugPrint(4, "CyU3PDeviceGpioOverride failed, error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }

        gpioConfig.intrMode = CY_U3P_GPIO_NO_INTR;

        if (pins[i].input) {
            // input config
            gpioConfig.outValue = CyTrue;
            gpioConfig.inputEn = CyTrue;
            gpioConfig.driveLowEn = CyFalse;
            gpioConfig.driveHighEn = CyFalse;
        } else {
            // output config
            gpioConfig.outValue = CyFalse;
            gpioConfig.driveLowEn = CyTrue;
            gpioConfig.driveHighEn = CyTrue;
            gpioConfig.inputEn = CyFalse;
        }

        apiRetStatus = CyU3PGpioSetSimpleConfig(pins[i].pin, &gpioConfig);
        if (apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "CyU3PGpioSetSimpleConfig failed, error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
    }
}

void UartBridgeStart(void)
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
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
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

void UartBridgeStop(void)
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

void CyFxGpioInit(void)
{
    CyU3PGpioClock_t gpioClock;
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Init the GPIO module */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 0;
    gpioClock.simpleDiv = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
    gpioClock.clkSrc = CY_U3P_SYS_CLK;
    gpioClock.halfDiv = 0;

    apiRetStatus = CyU3PGpioInit(&gpioClock, NULL);
    if (apiRetStatus != 0)
    {
        /* Error Handling */
        CyU3PDebugPrint (4, "CyU3PGpioInit failed, error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    NuandGPIOReconfigure(CyTrue, CyFalse);
}

int FpgaBeginProgram(void)
{
    CyBool_t value;

    int tEnd;
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

/* DMA callback function to handle the produce events for U to P transfers. */
void bladeRFConfigUtoPDmaCallback(CyU3PDmaChannel *chHandle, CyU3PDmaCbType_t type, CyU3PDmaCBInput_t *input)
{
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    if (type == CY_U3P_DMA_CB_PROD_EVENT) {
        int i;
        unsigned char *buf = input->buffer_p.buffer;
        unsigned *us_buf = (unsigned short *)input->buffer_p.buffer;
        unsigned tmpr, tmpw;

        /* Flip the bits in such a way that the FPGA can be programmed
         * This mapping can be determined by looking at the schematic */
        for (i = input->buffer_p.count - 1; i >= 0; i--) {
            buf[i * 4] = buf[i];
            buf[i * 4 + 1] = 0;
            buf[i * 4 + 2] = 0;
            buf[i * 4 + 3] = 0;
            tmpr = us_buf[i];

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
            us_buf[i] = tmpw;
        }
        status = CyU3PDmaChannelCommitBuffer (chHandle, input->buffer_p.count * 4, 0);
        if (status != CY_U3P_SUCCESS) {
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
        }

        /* Increment the counter. */
        glDMARxCount++;
    }
}

/* This function starts the RF data transport mechanism. This is the second
 * interface of the first and only descriptor. */
void NuandRFLinkStart(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PDmaMultiChannelConfig_t dmaMultiConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    NuandGPIOReconfigure(CyTrue, CyTrue);

    /* Load the GPIF configuration for loading the RF transceiver */
    apiRetStatus = CyU3PGpifLoad(&Rflink_CyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpifLoad failed, Error Code = %d\n",apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    // strobe the RESET pin to the FPGA
    CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
    CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

    /* Start the state machine. */
    apiRetStatus = CyU3PGpifSMStart(RFLINK_START, RFLINK_ALPHA_START);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PGpifSMStart failed, Error Code = %d\n",apiRetStatus);
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

#if TX_MULTI
    apiRetStatus = CyU3PDmaMultiChannelCreate(&glChHandleMultiUtoP, CY_U3P_DMA_TYPE_AUTO_ONE_TO_MANY, &dmaMultiConfig);
#else
    apiRetStatus = CyU3PDmaChannelCreate(&glChHandleUtoP, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
#endif

    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

#ifdef RX_MULTI
    dmaMultiConfig.prodSckId[0] = CY_U3P_PIB_SOCKET_0;
    dmaMultiConfig.prodSckId[1] = CY_U3P_PIB_SOCKET_1;
    dmaMultiConfig.consSckId[0] = BLADE_RF_SAMPLE_EP_CONSUMER_USB_SOCKET;
    dmaMultiConfig.consSckId[1] = 0;

    apiRetStatus = CyU3PDmaMultiChannelCreate(&glChHandleMultiPtoU, CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE, &dmaMultiConfig);
#else
    dmaCfg.prodSckId = CY_U3P_PIB_SOCKET_0;
    dmaCfg.consSckId = BLADE_RF_SAMPLE_EP_CONSUMER_USB_SOCKET;
    apiRetStatus = CyU3PDmaChannelCreate(&glChHandlePtoU, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
#endif
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Flush the Endpoint memory */
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_CONSUMER);

    /* Set DMA channel transfer size. */

#ifdef TX_MULTI
    apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glChHandleMultiUtoP, BLADE_DMA_TX_SIZE, 0);
#else
    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleUtoP, BLADE_DMA_TX_SIZE);
#endif

    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

#ifdef RX_MULTI
    apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glChHandleMultiPtoU, BLADE_DMA_TX_SIZE, 0);
#else
    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandlePtoU, BLADE_DMA_TX_SIZE);
#endif
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }


    UartBridgeStart();
    glAppMode = MODE_RF_CONFIG;

}

/* This function stops the slave FIFO loop application. This shall be called
 * whenever a RESET or DISCONNECT event is received from the USB host. The
 * endpoints are disabled and the DMA pipe is destroyed by this function. */
void NuandRFLinkStop (void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Flush endpoint memory buffers */
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_PRODUCER);
    CyU3PUsbFlushEp(BLADE_RF_SAMPLE_EP_CONSUMER);

    /* Destroy the channels */
#ifdef TX_MUTLI
    CyU3PDmaMultiChannelDestroy(&glChHandleMultiUtoP);
#else
    CyU3PDmaChannelDestroy(&glChHandleUtoP);
#endif
#ifdef RX_MULTI
    CyU3PDmaMultiChannelDestroy(&glChHandleMultiPtoU);
#else
    CyU3PDmaChannelDestroy(&glChHandlePtoU);
#endif

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
    CyU3PGpifDisable(CyTrue);

    UartBridgeStop();
    glAppMode = MODE_NO_CONFIG;
}

void NuandFpgaConfigStart(void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    NuandGPIOReconfigure(CyTrue, CyFalse);

    /* Load the GPIF configuration for loading the FPGA */
    apiRetStatus = CyU3PGpifLoad(&C4loader_CyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PGpifLoad failed, Error Code = %d\n",apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Start the state machine. */
    apiRetStatus = CyU3PGpifSMStart(C4LOADER_START, C4LOADER_ALPHA_START);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint(4, "CyU3PGpifSMStart failed, Error Code = %d\n",apiRetStatus);
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

    CyU3PGpifDisable(CyTrue);

    glAppMode = MODE_NO_CONFIG;
}

/* Callback to handle the USB setup requests. */
CyBool_t CyFxbladeRFApplnUSBSetupCB(uint32_t setupdat0, uint32_t setupdat1)
{
    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    CyBool_t isHandled = CyFalse;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);
    wLength  = ((setupdat1 & CY_U3P_USB_LENGTH_MASK)  >> CY_U3P_USB_LENGTH_POS);

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        if (bRequest == CY_U3P_USB_SC_GET_INTERFACE) {
                glEp0Buffer[0] = glUsbInterface;
                CyU3PUsbSendEP0Data(wLength, glEp0Buffer);
                isHandled = CyTrue;
        }

        /* Handle suspend requests */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0)) {
            if (glAppMode != MODE_NO_CONFIG)
                CyU3PUsbAckSetup();
            else
                CyU3PUsbStall(0, CyTrue, CyFalse);
            isHandled = CyTrue;
        }

        /* Flush enpoint memory and reset channel if CLEAR_FEATURE is received */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glAppMode != MODE_NO_CONFIG) {
#if 0
                if (wIndex == CY_FX_EP_PRODUCER) {
                    CyU3PDmaChannelReset (&glChHandlebladeRFUtoP);
                    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);
                    CyU3PUsbResetEp(CY_FX_EP_PRODUCER);
                    CyU3PDmaChannelSetXfer (&glChHandlebladeRFUtoP, BLADE_DMA_TX_SIZE);
                }

                if (wIndex == CY_FX_EP_CONSUMER) {
                    CyU3PDmaChannelReset (&glChHandlebladeRFPtoU);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp(CY_FX_EP_CONSUMER);
                    CyU3PDmaChannelSetXfer (&glChHandlebladeRFPtoU, BLADE_DMA_RX_SIZE);
                }
#endif

                CyU3PUsbStall(wIndex, CyFalse, CyTrue);
                isHandled = CyTrue;
            }
        }
    }


    /* Handle supported bladeRF vendor requests. */
    if (bType == CY_U3P_USB_VENDOR_RQT)
    {
        unsigned int ret;
        CyBool_t fpgaProg;
        struct bladeRF_version ver;
        CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
        int retStatus;
    	char buf[10];
    	uint16_t readC;

        isHandled = CyTrue;

        switch (bRequest)
        {
            case BLADE_USB_CMD_QUERY_VERSION:
                ver.major = 0;
                ver.minor = 3;
                apiRetStatus = CyU3PUsbSendEP0Data(sizeof(ver), &ver);
            break;

            case BLADE_USB_CMD_RF_RX:
                CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
                CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

            	apiRetStatus = CyU3PUsbGetEP0Data(4, buf, &readC);

                if (!(buf[0] || buf[3])) {
                    apiRetStatus = CyU3PUsbResetEp(BLADE_RF_SAMPLE_EP_CONSUMER);
                    //CyU3PThreadSleep(1000);
                }

            	CyU3PGpioSetValue(GPIO_RX_EN, (buf[0] || buf[3]) ? CyTrue : CyFalse);
            break;

            case BLADE_USB_CMD_RF_TX:
                CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
                CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

                apiRetStatus = CyU3PUsbGetEP0Data(4, buf, &readC);

                if (!(buf[0] || buf[3])) {
                    apiRetStatus = CyU3PUsbResetEp(BLADE_RF_SAMPLE_EP_PRODUCER);
                    //CyU3PThreadSleep(1000);
                }

                CyU3PGpioSetValue(GPIO_TX_EN, (buf[0] || buf[3]) ? CyTrue : CyFalse);
            break;

            case BLADE_USB_CMD_BEGIN_PROG:
                retStatus = FpgaBeginProgram();
                apiRetStatus = CyU3PUsbSendEP0Data (sizeof(retStatus), &retStatus);
                break;
            break;

            case BLADE_USB_CMD_QUERY_FPGA_STATUS:
                apiRetStatus = CyU3PGpioGetValue (GPIO_CONFDONE, &fpgaProg);
                if (apiRetStatus == CY_U3P_SUCCESS) {
                    ret = fpgaProg ? 1 : 0;
                } else {
                    ret = -1;
                }
                apiRetStatus = CyU3PUsbSendEP0Data (sizeof(ret), &ret);
            break;

            case BLADE_USB_CMD_WRITE_OTP:
                NuandEnso();
                if (CyU3PUsbGetSpeed() == CY_U3P_SUPER_SPEED) {
                    apiRetStatus = CyU3PUsbGetEP0Data(0x100, &glEp0Buffer, &readC);
                    apiRetStatus = CyFxSpiTransfer (0, 0x100,
                            glEp0Buffer, CyFalse);
                }
                NuandExso();
                NuandLockOtp();
            break;

            case BLADE_USB_CMD_READ_OTP:
                if (CyU3PUsbGetSpeed() == CY_U3P_HIGH_SPEED) {
                    if (glEp0Idx == 0) {
                        NuandEnso();
                        apiRetStatus = CyFxSpiTransfer (0, 0x100,
                                glEp0Buffer, CyTrue);
                        apiRetStatus = CyU3PUsbSendEP0Data(wLength, &glEp0Buffer);
                        NuandExso();
                    } else {
                        apiRetStatus = CY_U3P_SUCCESS;
                    }
                    apiRetStatus = CyU3PUsbSendEP0Data(wLength, &glEp0Buffer[glEp0Idx]);

                    glEp0Idx += 64;
                    if (glEp0Idx == 256)
                        glEp0Idx = 0;
                } else if (CyU3PUsbGetSpeed() == CY_U3P_SUPER_SPEED) {
                    NuandEnso();
                    apiRetStatus = CyFxSpiTransfer (0, 0x100,
                            glEp0Buffer, CyTrue);
                    apiRetStatus = CyU3PUsbSendEP0Data(0x100, &glEp0Buffer);
                    NuandExso();
                }
            break;

            case BLADE_USB_CMD_FLASH_READ:
                if (CyU3PUsbGetSpeed() == CY_U3P_HIGH_SPEED) {
                    if (glEp0Idx == 0) {
                        apiRetStatus = CyFxSpiTransfer (wIndex, 0x100,
                                glEp0Buffer, CyTrue);
                    } else {
                        apiRetStatus = CY_U3P_SUCCESS;
                    }
                    apiRetStatus = CyU3PUsbSendEP0Data(wLength, &glEp0Buffer[glEp0Idx]);

                    glEp0Idx += 64;
                    if (glEp0Idx == 256)
                        glEp0Idx = 0;
                } else if (CyU3PUsbGetSpeed() == CY_U3P_SUPER_SPEED) {
                    apiRetStatus = CyFxSpiTransfer (wIndex, 0x100,
                            glEp0Buffer, CyTrue);
                    apiRetStatus = CyU3PUsbSendEP0Data(0x100, &glEp0Buffer);
                }
            break;

            case BLADE_USB_CMD_FLASH_WRITE:
                if (CyU3PUsbGetSpeed() == CY_U3P_HIGH_SPEED) {
                    apiRetStatus = CyU3PUsbGetEP0Data(64, &glEp0Buffer[glEp0Idx], &readC);

                    glEp0Idx += 64;
                    if (glEp0Idx == 256){
                        apiRetStatus = CyFxSpiTransfer (wIndex, 0x100,
                                glEp0Buffer, CyFalse);
                        glEp0Idx = 0;
                    }
                } else if (CyU3PUsbGetSpeed() == CY_U3P_SUPER_SPEED) {
                    apiRetStatus = CyU3PUsbGetEP0Data(0x100, &glEp0Buffer, &readC);
                    apiRetStatus = CyFxSpiTransfer (wIndex, 0x100,
                            glEp0Buffer, CyFalse);
                }
            break;

            case BLADE_USB_CMD_FLASH_ERASE:
                apiRetStatus = CyFxSpiEraseSector(CyTrue, wIndex);
                ret = (apiRetStatus == CY_U3P_SUCCESS);
                CyU3PUsbSendEP0Data(sizeof(ret), &ret);
            break;

            break;
            default:
                isHandled = CyFalse;

        }
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. */
void CyFxbladeRFApplnUSBEventCB (CyU3PUsbEventType_t evtype, uint16_t evdata)
{
    int interface;
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETINTF:
            interface = evdata >> 8;

            /* Don't do anything if we're setting the same interface over */
            if( interface == glUsbInterface ) break ;

            /* Stop whatever we were doing */
            switch(glUsbInterface) {
                case 0: NuandFpgaConfigStop() ; break ;
                case 1: NuandRFLinkStop(); break ;
                default: break ;
            }

            /* Start up the new one */
            if (interface == 0) {
                NuandFpgaConfigStart();
            } else if (interface == 1) {
                NuandRFLinkStart();
            } else if (interface == 2) {
                glEp0Idx = 0;
                NuandFirmwareStart();
            }
            glUsbInterface = interface;
        break;

        case CY_U3P_USB_EVENT_SETCONF:
            /* Stop the application before re-starting. */
            switch (glAppMode) {
            case MODE_RF_CONFIG:
                NuandRFLinkStop();
            case MODE_NO_CONFIG:
                NuandFpgaConfigStart();
            case MODE_FPGA_CONFIG:
                FpgaBeginProgram();
            }
            break;

        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            /* Stop the loop back function. */
            if (glAppMode == MODE_RF_CONFIG) {
                NuandRFLinkStop();
            } else if (glAppMode == MODE_FPGA_CONFIG){
                NuandFpgaConfigStop();
            }
            break;

        default:
            break;
    }
}

/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.

   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t
CyFxApplnLPMRqtCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return CyTrue;
}

void bladeRFInit(void)
{
    CyU3PPibClock_t pibClock;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Initialize the p-port block. */
    pibClock.clkDiv = 4;
    pibClock.clkSrc = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv = CyFalse;
    /* Enable DLL for async GPIF */
    pibClock.isDllEnable = CyFalse;
    apiRetStatus = CyU3PPibInit(CyTrue, &pibClock);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "P-port Initialization failed, Error Code = %d\n",apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Start the USB functionality. */
    apiRetStatus = CyU3PUsbStart();
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "CyU3PUsbStart failed to Start, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* The fast enumeration is the easiest way to setup a USB connection,
     * where all enumeration phase is handled by the library. Only the
     * class / vendor requests need to be handled by the application. */
    CyU3PUsbRegisterSetupCallback(CyFxbladeRFApplnUSBSetupCB, CyFalse);

    /* Setup the callback to handle the USB events. */
    CyU3PUsbRegisterEventCallback(CyFxbladeRFApplnUSBEventCB);

    /* Register a callback to handle LPM requests from the USB 3.0 host. */
    CyU3PUsbRegisterLPMRequestCallback(CyFxApplnLPMRqtCB);

    /* Set the USB Enumeration descriptors */

    /* Super speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, NULL, (uint8_t *)CyFxUSB30DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, NULL, (uint8_t *)CyFxUSB20DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* BOS descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, NULL, (uint8_t *)CyFxUSBBOSDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Device qualifier descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, NULL, (uint8_t *)CyFxUSBDeviceQualDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device qualifier descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Super speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, NULL, (uint8_t *)CyFxUSBSSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, NULL, (uint8_t *)CyFxUSBHSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB Set Other Speed Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Full speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, NULL, (uint8_t *)CyFxUSBFSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB Set Configuration Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 0 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 1 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 2 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Connect the USB Pins with super speed operation enabled. */
    apiRetStatus = CyU3PConnectState(CyTrue, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB Connect failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    glAppMode = MODE_NO_CONFIG;
}

/* Entry function for the bladeRFAppThread. */
void bladeRFAppThread_Entry( uint32_t input)
{
    uint8_t state;
    CyFxGpioInit();

    bladeRFInit();

    FpgaBeginProgram();

    for (;;) {
        CyU3PThreadSleep (100);
        CyU3PGpifGetSMState(&state);
    }
}

/* Application define function which creates the threads. */
void CyFxApplicationDefine(void)
{
    void *ptr = NULL;
    uint32_t retThrdCreate = CY_U3P_SUCCESS;

    /* Allocate the memory for the threads */
    ptr = CyU3PMemAlloc(BLADE_THREAD_STACK);

    /* Create the thread for the application */
    retThrdCreate = CyU3PThreadCreate(&bladeRFAppThread,    /* Slave FIFO app thread structure */
                          "21:bladeRF_thread",               /* Thread ID and thread name */
                          bladeRFAppThread_Entry,            /* Slave FIFO app thread entry function */
                          0,                                 /* No input parameter to thread */
                          ptr,                               /* Pointer to the allocated thread stack */
                          BLADE_THREAD_STACK,                /* App Thread stack size */
                          BLADE_THREAD_PRIORITY,             /* App Thread priority */
                          BLADE_THREAD_PRIORITY,             /* App Thread pre-emption threshold */
                          CYU3P_NO_TIME_SLICE,               /* No time slice for the application thread */
                          CYU3P_AUTO_START                   /* Start the thread immediately */
                          );

    if (retThrdCreate != 0) {
        /* Application cannot continue */
        /* Loop indefinitely */
        while(1);
    }
}

/*
 * Main function
 */
int main(void)
{
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Initialize the device */
    status = CyU3PDeviceInit(NULL);
    if (status != CY_U3P_SUCCESS) {
        goto handle_fatal_error;
    }

    /* Initialize the caches. Enable both Instruction and Data Caches. */
    status = CyU3PDeviceCacheControl(CyTrue, CyTrue, CyTrue);
    if (status != CY_U3P_SUCCESS) {
        goto handle_fatal_error;
    }

    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyFalse;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.isDQ32Bit = CyTrue;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_DEFAULT;

    /* No GPIOs are enabled. */
    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = 0;
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
    status = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* This is a non returnable call for initializing the RTOS kernel */
    CyU3PKernelEntry();

    /* Dummy return to make the compiler happy */
    return 0;

handle_fatal_error:
    /* Cannot recover from this error. */
    while (1);
}
