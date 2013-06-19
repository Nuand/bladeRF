
#define BLADERF_IOCTL_BASE      'N'
#define BLADE_QUERY_VERSION     _IOR(BLADERF_IOCTL_BASE, 0, struct bladeRF_version)
#define BLADE_QUERY_FPGA_STATUS     _IOR(BLADERF_IOCTL_BASE, 1, unsigned int)
#define BLADE_BEGIN_PROG        _IOR(BLADERF_IOCTL_BASE, 2, unsigned int)
#define BLADE_END_PROG          _IOR(BLADERF_IOCTL_BASE, 3, unsigned int)
#define BLADE_CHECK_PROG        _IOR(BLADERF_IOCTL_BASE, 4, unsigned int)
#define BLADE_RF_RX             _IOR(BLADERF_IOCTL_BASE, 5, unsigned int)
#define BLADE_RF_TX             _IOR(BLADERF_IOCTL_BASE, 6, unsigned int)

#define BLADE_UPGRADE_FW        _IOR(BLADERF_IOCTL_BASE, 50, unsigned int)

#define BLADE_USB_CMD_QUERY_VERSION             0
#define BLADE_USB_CMD_QUERY_FPGA_STATUS         1
#define BLADE_USB_CMD_BEGIN_PROG                2
#define BLADE_USB_CMD_END_PROG                  3
#define BLADE_USB_CMD_RF_RX                     4
#define BLADE_USB_CMD_RF_TX                     5
#define BLADE_USB_CMD_FLASH_READ              100
#define BLADE_USB_CMD_FLASH_WRITE             101
#define BLADE_USB_CMD_FLASH_ERASE             102

#define BLADE_USB_CMD_QUERY_VERSION      0
#define BLADE_USB_CMD_QUERY_FPGA_STATUS  1
#define BLADE_USB_CMD_BEGIN_PROG         2
#define BLADE_USB_CMD_END_PROG           3
#define BLADE_USB_CMD_RF_RX              4

struct bladeRF_version {
    unsigned short major;
    unsigned short minor;
};

struct bladeRF_firmware {
    unsigned int len;
    unsigned char *ptr;
};

#define BLADE_USB_TYPE_OUT      0x40
#define BLADE_USB_TYPE_IN       0xC0
#define BLADE_USB_TIMEOUT_MS    1000

#define USB_NUAND_VENDOR_ID     0x1d50
#define USB_NUAND_BLADERF_PRODUCT_ID    0x6066
#define NUM_CONCURRENT  8
#define NUM_DATA_URB    (128)
#define DATA_BUF_SZ     (1024*4)

