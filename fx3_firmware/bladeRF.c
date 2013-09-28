/*
 * bladeRF FX3 firmware (bladeRF.c)
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "bladeRF.h"
#include "cyu3gpif.h"
#include "cyu3pib.h"
#include "cyu3gpio.h"
#include "pib_regs.h"

#include "firmware.h"
#include "spi_flash_lib.h"
#include "rf.h"
#include "fpga.h"


uint32_t glAppMode = MODE_NO_CONFIG;

CyU3PThread bladeRFAppThread;

uint8_t glUsbConfiguration = 0;             /* Active USB configuration. */
uint8_t glUsbAltInterface = 0;                 /* Active USB interface. */


uint8_t glSelBuffer[32];
uint8_t glPageBuffer[FLASH_PAGE_SIZE] __attribute__ ((aligned (32)));

CyBool_t glCalCacheValid = CyFalse;
uint8_t glCal[CAL_BUFFER_SIZE] __attribute__ ((aligned (32)));

/* Standard product string descriptor */
uint8_t CyFxUSBSerial[] __attribute__ ((aligned (32))) =
{
    0x42,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    '0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,
    '0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,
    '0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,
    '0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,'0',0x00,
};

/* Application Error Handler */
void CyFxAppErrorHandler(CyU3PReturnStatus_t apiRetStatus)
{
    /* firmware failed with the error code apiRetStatus */

    /* Loop Indefinitely */
    for (;;)
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep(100);
}

static int FpgaBeginProgram(void)
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


void NuandGPIOReconfigure(CyBool_t fullGpif, CyBool_t warm)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
    size_t i;

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

void CyFxGpioInit(void)
{
    CyU3PGpioClock_t gpioClock;
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

static void StopApplication()
{
    if (glAppMode == MODE_RF_CONFIG) {
        NuandRFLink.stop();
    } else if (glAppMode == MODE_FPGA_CONFIG){
        NuandFpgaConfig.stop();
    }
}

CyBool_t GetStatus(uint16_t endpoint) {
    CyBool_t isHandled = CyFalse;
    uint8_t get_status_reply[] = {0x00, 0x00};

    switch(glUsbAltInterface) {
    case USB_IF_RF_LINK:
        isHandled = NuandRFLink.halted(endpoint, &get_status_reply[0]);
        break;
    case USB_IF_CONFIG:
        isHandled = NuandFpgaConfig.halted(endpoint, &get_status_reply[0]);
        break;
    default:
    case USB_IF_SPI_FLASH:
        /* USB_IF_CONFIG has no end points */
        break;
    }

    if(isHandled) {
        CyU3PUsbSendEP0Data(sizeof(get_status_reply), get_status_reply);
    }

    return isHandled;
}

void ClearDMAChannel(uint8_t ep, CyU3PDmaChannel * handle, uint32_t count, CyBool_t stall_only) {
    CyU3PDmaChannelReset (handle);
    CyU3PUsbFlushEp(ep);
    CyU3PUsbResetEp(ep);
    CyU3PDmaChannelSetXfer (handle, count);
    CyU3PUsbStall (ep, CyFalse, CyTrue);

    if(!stall_only) {
        CyU3PUsbAckSetup ();
    }
}

CyBool_t ClearHaltCondition(uint16_t endpoint) {
    CyBool_t isHandled = CyFalse;

    switch(glUsbAltInterface) {
    case USB_IF_RF_LINK:
        isHandled = NuandRFLink.halt_endpoint(CyFalse, endpoint);
        break;
    case USB_IF_CONFIG:
        isHandled = NuandFpgaConfig.halt_endpoint(CyFalse, endpoint);
        break;
    default:
    case USB_IF_SPI_FLASH:
        /* USB_IF_SPI_FLASH has no end points */
        break;
    }

    return isHandled;
}

CyBool_t SetHaltCondition(uint16_t endpoint) {
    CyBool_t isHandled = CyFalse;

    switch(glUsbAltInterface) {
    case USB_IF_RF_LINK:
        isHandled = NuandRFLink.halt_endpoint(CyTrue, endpoint);
        break;
    case USB_IF_CONFIG:
        isHandled = NuandFpgaConfig.halt_endpoint(CyTrue, endpoint);
        break;
    default:
    case USB_IF_SPI_FLASH:
        /* USB_IF_CONFIG has no end points */
        break;
    }

    return isHandled;
}

void CyU3PUsbSendRetCode(CyU3PReturnStatus_t ret_status) {
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = CyU3PUsbSendEP0Data(
            sizeof(ret_status),
            (void*)&ret_status);

    if(apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4,
                "Failed to return the error code to host, error code = %d\n",
                apiRetStatus);
    }
}

static CyU3PReturnStatus_t NuandReadCalTable(uint8_t *cal_buff) {
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = CyFxSpiTransfer(CAL_PAGE, CAL_BUFFER_SIZE, cal_buff, CyTrue);

    /* FIXME: Validate table */

    return apiRetStatus;
}

CyBool_t NuandHandleVendorRequest(
        uint8_t bRequest,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength
        ) {
    CyBool_t isHandled;
    unsigned int ret;
    uint8_t use_feature;
    CyBool_t fpgaProg;
    struct bladeRF_version ver;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    int retStatus;
    uint16_t readC;

    isHandled = CyTrue;

    switch (bRequest)
    {
    case BLADE_USB_CMD_QUERY_VERSION:
        ver.major = 1;
        ver.minor = 4;
        apiRetStatus = CyU3PUsbSendEP0Data(sizeof(ver), (void*)&ver);
    break;

    case BLADE_USB_CMD_RF_RX:
        CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
        CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

        apiRetStatus = CY_U3P_SUCCESS;
        use_feature = wValue;

        if (!use_feature) {
            apiRetStatus = CyU3PUsbResetEp(BLADE_RF_SAMPLE_EP_CONSUMER);
            if(apiRetStatus != CY_U3P_SUCCESS) {
                CyU3PDebugPrint (4,
                        "Failed reset USB, error code = %d\n",
                        apiRetStatus);
            }
        }

        apiRetStatus = CyU3PGpioSetValue(GPIO_RX_EN, use_feature ? CyTrue : CyFalse);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_RF_TX:
        CyU3PGpioSetValue(GPIO_SYS_RST, CyTrue);
        CyU3PGpioSetValue(GPIO_SYS_RST, CyFalse);

        apiRetStatus = CY_U3P_SUCCESS;
        use_feature = wValue;

        if (!use_feature) {
            apiRetStatus = CyU3PUsbResetEp(BLADE_RF_SAMPLE_EP_PRODUCER);
            if(apiRetStatus != CY_U3P_SUCCESS) {
                CyU3PDebugPrint (4,
                        "Failed reset USB, error code = %d\n",
                        apiRetStatus);
            }
        }

        apiRetStatus = CyU3PGpioSetValue(GPIO_TX_EN, use_feature ? CyTrue : CyFalse);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_BEGIN_PROG:
        retStatus = FpgaBeginProgram();
        CyU3PUsbSendRetCode(retStatus);
    break;

    case BLADE_USB_CMD_QUERY_FPGA_STATUS:
        apiRetStatus = CyU3PGpioGetValue (GPIO_CONFDONE, &fpgaProg);
        if (apiRetStatus == CY_U3P_SUCCESS) {
            ret = fpgaProg ? 1 : 0;
        } else {
            ret = -1;
        }

        CyU3PUsbSendRetCode(ret);
    break;

    case BLADE_USB_CMD_READ_PAGE_BUFFER:
        if(wIndex + wLength > sizeof(glPageBuffer)) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = CyU3PUsbSendEP0Data(wLength, &glPageBuffer[wIndex]);
        if(apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "Failed to send data, error code = %d\n", apiRetStatus);
        }
    break;

    case BLADE_USB_CMD_WRITE_PAGE_BUFFER:
        if(wIndex + wLength > sizeof(glPageBuffer)) {
            apiRetStatus = CyU3PUsbStall(0x0, CyTrue, CyFalse);
        }

        apiRetStatus = CyU3PUsbGetEP0Data(wLength, &glPageBuffer[wIndex], &readC);
        if(apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "Failed to get data, error code = %d\n", apiRetStatus);
        }

        if(readC != wLength) {
            CyU3PDebugPrint(4, "Failed to get data, got %u, expected %u\n",
                readC, wLength);
        }
    break;

    case BLADE_USB_CMD_WRITE_OTP:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = NuandWriteOtp(wIndex, 0x100, glPageBuffer);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_READ_OTP:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = NuandReadOtp(wIndex, 0x100, glPageBuffer);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_LOCK_OTP:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = NuandLockOtp();
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_FLASH_READ:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = CyFxSpiTransfer (
                wIndex, 0x100,
                glPageBuffer, CyTrue);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_FLASH_WRITE:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = CyFxSpiTransfer (
                wIndex, 0x100,
                glPageBuffer, CyFalse);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_FLASH_ERASE:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
           apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = CyFxSpiEraseSector(CyTrue, wIndex);
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_READ_CAL_CACHE:
        if(!glCalCacheValid) {
            /* Fail the request if the cache is invalid */
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        if(wIndex + wLength > sizeof(glCal)) {
            apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = CyU3PUsbSendEP0Data(wLength, &glCal[wIndex]);
        if(apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PDebugPrint(4, "Failed to send data, error code = %d\n", apiRetStatus);
        }
    break;

    case BLADE_USB_CMD_INVALIDATE_CAL_CACHE:
        glCalCacheValid = CyFalse;
        CyU3PUsbAckSetup();
    break;

    case BLADE_USB_CMD_REFRESH_CAL_CACHE:
        if (glUsbAltInterface != USB_IF_SPI_FLASH) {
           apiRetStatus = CyU3PUsbStall(0x80, CyTrue, CyFalse);
        }

        apiRetStatus = NuandReadCalTable(glCal);
        if(apiRetStatus == CY_U3P_SUCCESS) {
            glCalCacheValid = CyTrue;
        }
        CyU3PUsbSendRetCode(apiRetStatus);
    break;

    case BLADE_USB_CMD_RESET:
        CyU3PUsbAckSetup();
        CyU3PDeviceReset(CyFalse);
    break;

    case BLADE_USB_CMD_JUMP_TO_BOOTLOADER:
        StopApplication();
        NuandFirmwareStart();

        // Erase the first sector so we can write the bootloader 
        // header
        apiRetStatus = CyFxSpiEraseSector(CyTrue, 0);
        if(apiRetStatus != CY_U3P_SUCCESS) {
            CyU3PUsbStall(0, CyTrue, CyFalse);
            CyU3PDeviceReset(CyFalse);
            break;
        }

        uint8_t bootloader_header[] = {
            'C','Y', // Common header
            0,       // 10 MHz SPI, image data
            0xB2,    // Bootloader VID/PID
            (USB_NUAND_BLADERF_BOOT_PRODUCT_ID & 0xFF),
            (USB_NUAND_BLADERF_BOOT_PRODUCT_ID & 0xFF00) >> 8,
            (USB_NUAND_VENDOR_ID & 0xFF),
            (USB_NUAND_VENDOR_ID & 0xFF00) >> 8,
        };

        apiRetStatus = CyFxSpiTransfer (
                /* Page */ 0,
                sizeof(bootloader_header), bootloader_header,
                CyFalse /* Writing */
                );

        CyU3PUsbAckSetup();
        CyU3PDeviceReset(CyFalse);
    break;

    default:
        isHandled = CyFalse;
    }

    return isHandled;
}

/* Callback to handle the USB setup requests. */
CyBool_t CyFxbladeRFApplnUSBSetupCB(uint32_t setupdat0, uint32_t setupdat1)
{
    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    uint8_t bTemp;
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
        if (bRequest == CY_U3P_USB_SC_SET_SEL)
        {
            {
                if ((CyU3PUsbGetSpeed () == CY_U3P_SUPER_SPEED) && (wValue == 0) && (wIndex == 0) && (wLength == 6))
                {
                    CyU3PUsbGetEP0Data (32, glSelBuffer, 0);
                }
                else
                {
                    isHandled = CyFalse;
                }
            }
        }


        if (bRequest == CY_U3P_USB_SC_GET_INTERFACE && wLength == 1) {
                bTemp = glUsbAltInterface;
                CyU3PUsbSendEP0Data(wLength, &bTemp);
                isHandled = CyTrue;
        }

        if (bRequest == CY_U3P_USB_SC_GET_CONFIGURATION && wLength == 1) {
                bTemp = glUsbConfiguration;
                CyU3PUsbSendEP0Data(wLength, &bTemp);
                isHandled = CyTrue;
        }

        /* Handle suspend requests */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0)) {
            if (glUsbConfiguration != 0) {
                /* FIXME: Actually implement suspend/resume requests */
                CyU3PUsbAckSetup();
            } else {
                CyU3PUsbStall(0, CyTrue, CyFalse);
            }
            isHandled = CyTrue;
        }

        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_GET_STATUS))
        {
            if (glAppMode != MODE_NO_CONFIG) {
                isHandled = GetStatus(wIndex);
            }
        }

        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glAppMode != MODE_NO_CONFIG) {
                isHandled = ClearHaltCondition(wIndex);
            }
        }

        /* Flush endpoint memory and reset channel if CLEAR_FEATURE is received */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_SET_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glAppMode != MODE_NO_CONFIG) {
                isHandled = SetHaltCondition(wIndex);
            }
        }
    }

    /* Handle supported bladeRF vendor requests. */
    if (bType == CY_U3P_USB_VENDOR_RQT)
    {
        isHandled = NuandHandleVendorRequest(bRequest, wValue, wIndex, wLength);
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. */
void CyFxbladeRFApplnUSBEventCB (CyU3PUsbEventType_t evtype, uint16_t evdata)
{
    int interface;
    int alt_interface;
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETINTF:
            interface = (evdata & 0xf0) >> 4;
            alt_interface = evdata & 0xf;

            /* Only support sets to interface 0 for now */
            if(interface != 0) break;

            /* Don't do anything if we're setting the same interface over */
            if( alt_interface == glUsbAltInterface ) break ;

            /* Stop whatever we were doing */
            switch(glUsbAltInterface) {
                case USB_IF_CONFIG: NuandFpgaConfig.stop() ; break ;
                case USB_IF_RF_LINK: NuandRFLink.stop(); break ;
                case USB_IF_SPI_FLASH: NuandFirmwareStop(); break ;
                default: break ;
            }

            /* Start up the new one */
            if (alt_interface == USB_IF_CONFIG) {
                NuandFpgaConfig.start();
            } else if (alt_interface == USB_IF_RF_LINK) {
                NuandRFLink.start();
            } else if (alt_interface == USB_IF_SPI_FLASH) {
                NuandFirmwareStart();
            }
            glUsbAltInterface = alt_interface;
        break;

        case CY_U3P_USB_EVENT_SETCONF:
            glUsbConfiguration = evdata;
            break;

        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            /* Stop the loop back function. */
            StopApplication();
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

static CyBool_t allow_suspend = CyTrue;

void NuandAllowSuspend(CyBool_t set_allow_suspend) {
    allow_suspend = set_allow_suspend;
}

static CyBool_t CyFxApplnLPMRqtCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return allow_suspend;
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
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB30DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* BOS descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)CyFxUSBBOSDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Device qualifier descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)CyFxUSBDeviceQualDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set device qualifier descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Super speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBSSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBHSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS) {
        CyU3PDebugPrint (4, "USB Set Other Speed Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Full speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBFSConfigDscr);
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

    /* String descriptor 3 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 3, (uint8_t *)CyFxUSBSerial);
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

static uint8_t otp_buf[0x100];
static void ExtractSerial(void)
{
    int status;
    char serial_buf[32];
    int i;

    NuandFirmwareStart();

    status = NuandReadOtp(0, 0x100, otp_buf);

    if (!NuandExtractField((void*)otp_buf, 0x100, "S", serial_buf, 32)) {
        for (i = 0; i < 32; i++) {
            CyFxUSBSerial[2+i*2] = serial_buf[i];
        }
    }

    /* Initialize calibration table cache */
    status = NuandReadCalTable(glCal);
    if(status == CY_U3P_SUCCESS) {
        glCalCacheValid = CyTrue;
    }

    NuandFirmwareStop();
}

void bladeRFAppThread_Entry( uint32_t input)
{
    uint8_t state;
    CyFxGpioInit();

    bladeRFInit();

    ExtractSerial();

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

    NuandFpgaConfigSwInit();

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
