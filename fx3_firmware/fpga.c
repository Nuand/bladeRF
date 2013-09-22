#include "bladeRF.h"
#include "cyu3error.h"
#include "cyu3gpio.h"
#include "cyu3usb.h"
#include "cyu3gpif.h"
#include "cyfxgpif_C4loader.h"

/* DMA Channel for RF U2P (USB to P-port) transfers */
static CyU3PDmaChannel glChHandlebladeRFUtoP;

static uint32_t glDMARxCount = 0;                  /* Counter to track the number of buffers received
                                             * from USB during FPGA programming */
static uint16_t glFlipLut[256];


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
        uint32_t *end_in_w = &( ((uint32_t *)input->buffer_p.buffer)[input->buffer_p.count - 1]);


        /* Flip the bits in such a way that the FPGA can be programmed
         * This mapping can be determined by looking at the schematic */
        for (i = input->buffer_p.count - 1; i >= 0; i--) {
            *end_in_w-- = glFlipLut[*end_in_b--];
        }
        status = CyU3PDmaChannelCommitBuffer (chHandle, input->buffer_p.count * 4, 0);
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

    NuandGPIOReconfigure(CyTrue, !first_call);
    first_call = 0;


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

    NuandAllowSuspend(CyTrue);
    glAppMode = MODE_NO_CONFIG;
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

const struct NuandApplication NuandFpgaConfig = {
    .start = NuandFpgaConfigStart,
    .stop = NuandFpgaConfigStop,
    .halt_endpoint = NuandFpgaConfigHaltEndpoint,
    .halted = NuandFpgaConfigHalted,
};
