#ifndef _INCLUDED_BLADE_H_
#define _INCLUDED_BLADE_H_

#include "cyu3externcstart.h"
#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "../firmware_common/bladeRF.h"

#define BLADE_GPIF_16_32BIT_CONF_SELECT (0)

#define BLADE_DMA_BUF_COUNT      (2)
#define BLADE_DMA_TX_SIZE        (0)
#define BLADE_DMA_RX_SIZE        (0)
#define BLADE_THREAD_STACK       (0x0400)
#define BLADE_THREAD_PRIORITY    (8)

// interface #0
#define BLADE_FPGA_EP_PRODUCER          0x02
#define BLADE_FPGA_CONFIG_SOCKET        CY_U3P_UIB_SOCKET_PROD_2

// interface #1
#define BLADE_RF_SAMPLE_EP_PRODUCER     0x01
#define BLADE_RF_SAMPLE_EP_PRODUCER_USB_SOCKET CY_U3P_UIB_SOCKET_PROD_1
#define BLADE_UART_EP_PRODUCER          0x02
#define BLADE_UART_EP_PRODUCER_USB_SOCKET CY_U3P_UIB_SOCKET_PROD_2

#define BLADE_RF_SAMPLE_EP_CONSUMER     0x81
#define BLADE_RF_SAMPLE_EP_CONSUMER_USB_SOCKET CY_U3P_UIB_SOCKET_CONS_1
#define BLADE_UART_EP_CONSUMER          0x82
#define BLADE_UART_EP_CONSUMER_USB_SOCKET CY_U3P_UIB_SOCKET_CONS_2

// interface #2
// doesn't actually use BULK, EP exists for debugging purposes

/* Extern definitions for the USB Descriptors */
extern const uint8_t CyFxUSB20DeviceDscr[];
extern const uint8_t CyFxUSB30DeviceDscr[];
extern const uint8_t CyFxUSBDeviceQualDscr[];
extern const uint8_t CyFxUSBFSConfigDscr[];
extern const uint8_t CyFxUSBHSConfigDscr[];
extern const uint8_t CyFxUSBBOSDscr[];
extern const uint8_t CyFxUSBSSConfigDscr[];
extern const uint8_t CyFxUSBStringLangIDDscr[];
extern const uint8_t CyFxUSBManufactureDscr[];
extern const uint8_t CyFxUSBProductDscr[];

#include "cyu3externcend.h"

int FpgaBeginProgram(void);
void NuandFirmwareStart(void);
void NuandFirmwareStop(void);
CyU3PReturnStatus_t CyFxSpiEraseSector(CyBool_t /* isErase */, uint8_t /* sector */);
void NuandEnso();
void NuandExso();
void NuandLockOtp();
void NuandGPIOReconfigure(CyBool_t /* fullGpif */, CyBool_t /* warm */);
int FpgaBeginProgram(void);

extern uint32_t glAppMode;

#define GPIO_nSTATUS    52
#define GPIO_nCONFIG    51
#define GPIO_CONFDONE   50

#define GPIO_LED        58
#define GPIO_ID         59

#define GPIO_SYS_RST    24
#define GPIO_RX_EN      21
#define GPIO_TX_EN      22

#define MODE_NO_CONFIG   0
#define MODE_FPGA_CONFIG 1
#define MODE_RF_CONFIG   2
#define MODE_FW_CONFIG   3


#endif
