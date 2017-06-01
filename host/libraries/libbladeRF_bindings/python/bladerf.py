import enum
import collections

import cffi

ffi = cffi.FFI()

ffi.cdef("""
/** This structure is an opaque device handle */
struct bladerf;

typedef enum {
    BLADERF_BACKEND_ANY,    /**< "Don't Care" -- use any available backend */
    BLADERF_BACKEND_LINUX,  /**< Linux kernel driver */
    BLADERF_BACKEND_LIBUSB, /**< libusb */
    BLADERF_BACKEND_CYPRESS, /**< CyAPI */
    BLADERF_BACKEND_DUMMY = 100, /**< Dummy used for development purposes */
} bladerf_backend;

/** Length of device serial number string, including NUL-terminator */
#define BLADERF_SERIAL_LENGTH   33

struct bladerf_devinfo {
    bladerf_backend backend;    /**< Backend to use when connecting to device */
    char serial[BLADERF_SERIAL_LENGTH]; /**< Device serial number string */
    uint8_t usb_bus;            /**< Bus # device is attached to */
    uint8_t usb_addr;           /**< Device address on bus */
    unsigned int instance;      /**< Device instance or ID */
};

int bladerf_open(struct bladerf **device, const char *device_identifier);
void bladerf_close(struct bladerf *device);
int bladerf_open_with_devinfo(struct bladerf **device, struct bladerf_devinfo *devinfo);
int bladerf_get_device_list(struct bladerf_devinfo **devices);
void bladerf_free_device_list(struct bladerf_devinfo *devices);
void bladerf_init_devinfo(struct bladerf_devinfo *info);
int bladerf_get_devinfo(struct bladerf *dev, struct bladerf_devinfo *info);
int bladerf_get_devinfo_from_str(const char *devstr, struct bladerf_devinfo *info);
bool bladerf_devinfo_matches(const struct bladerf_devinfo *a, const struct bladerf_devinfo *b);
bool bladerf_devstr_matches(const char *dev_str, struct bladerf_devinfo *info);

const char * bladerf_backend_str(bladerf_backend backend);

void bladerf_set_usb_reset_on_open(bool enabled);

struct bladerf_version {
    uint16_t major;             /**< Major version */
    uint16_t minor;             /**< Minor version */
    uint16_t patch;             /**< Patch version */
    const char *describe;       /**< Version string */
};

typedef enum {
    BLADERF_FPGA_UNKNOWN = 0,   /**< Unable to determine FPGA variant */
    BLADERF_FPGA_40KLE = 40,    /**< 40 kLE FPGA */
    BLADERF_FPGA_115KLE = 115   /**< 115 kLE FPGA */
} bladerf_fpga_size;

typedef enum {
    BLADERF_DEVICE_SPEED_UNKNOWN,
    BLADERF_DEVICE_SPEED_HIGH,
    BLADERF_DEVICE_SPEED_SUPER,
} bladerf_dev_speed;

int bladerf_get_serial(struct bladerf *dev, char *serial);
int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size *size);
int bladerf_fw_version(struct bladerf *dev, struct bladerf_version *version);
int bladerf_is_fpga_configured(struct bladerf *dev);

int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version *version);
bladerf_dev_speed bladerf_device_speed(struct bladerf *dev);
const char * bladerf_get_board_name(struct bladerf *dev);

typedef int bladerf_channel;

struct bladerf_range {
    int64_t min;    /**< Minimum value */
    int64_t max;    /**< Maximum value */
    int64_t step;   /**< Step of value */
    float scale;    /**< Unit scale */
};

typedef enum  {
    BLADERF_GAIN_DEFAULT,           /**< Device-specific default */
    BLADERF_GAIN_MGC,               /**< Manual gain control */
    BLADERF_GAIN_FASTATTACK_AGC,    /**< Automatic gain control with fast attack (AD9361) */
    BLADERF_GAIN_SLOWATTACK_AGC,    /**< Automatic gain control with slow attack (AD9361) */
    BLADERF_GAIN_HYBRID_AGC         /**< Automatic gain control with hybrid attack (AD9361) */
} bladerf_gain_mode;

int bladerf_set_gain(struct bladerf *dev, bladerf_channel ch, int gain);
int bladerf_get_gain(struct bladerf *dev, bladerf_channel ch, int *gain);
int bladerf_set_gain_mode(struct bladerf *dev, bladerf_channel ch, bladerf_gain_mode mode);
int bladerf_get_gain_mode(struct bladerf *dev, bladerf_channel ch, bladerf_gain_mode *mode);
int bladerf_get_gain_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range);
int bladerf_set_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int gain);
int bladerf_get_gain_stage(struct bladerf *dev, bladerf_channel ch, const char *stage, int *gain);
int bladerf_get_gain_stage_range(struct bladerf *dev, bladerf_channel ch, const char *stage, struct bladerf_range *range);
int bladerf_get_gain_stages(struct bladerf *dev, bladerf_channel ch, const char **stages, unsigned int count);

struct bladerf_rational_rate {
    uint64_t integer;           /**< Integer portion */
    uint64_t num;               /**< Numerator in fractional portion */
    uint64_t den;               /**< Denominator in fractional portion. This
                                     must be > 0. */
};

int bladerf_set_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int rate, unsigned int *actual);
int bladerf_set_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual);
int bladerf_get_sample_rate(struct bladerf *dev, bladerf_channel ch, unsigned int *rate);
int bladerf_get_sample_rate_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range);
int bladerf_get_rational_sample_rate(struct bladerf *dev, bladerf_channel ch, struct bladerf_rational_rate *rate);
int bladerf_set_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int bandwidth, unsigned int *actual);
int bladerf_get_bandwidth(struct bladerf *dev, bladerf_channel ch, unsigned int *bandwidth);
int bladerf_get_bandwidth_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range);

int bladerf_select_band(struct bladerf *dev, bladerf_channel ch, uint64_t frequency);
int bladerf_set_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t frequency);
int bladerf_get_frequency(struct bladerf *dev, bladerf_channel ch, uint64_t *frequency);
int bladerf_get_frequency_range(struct bladerf *dev, bladerf_channel ch, struct bladerf_range *range);

int bladerf_set_rf_port(struct bladerf *dev, bladerf_channel ch, const char *port);
int bladerf_get_rf_port(struct bladerf *dev, bladerf_channel ch, const char **port);
int bladerf_get_rf_ports(struct bladerf *dev, bladerf_channel ch, const char **ports, unsigned int count);

typedef enum {
    BLADERF_LB_FIRMWARE = 1,
    BLADERF_LB_BB_TXLPF_RXVGA2,
    BLADERF_LB_BB_TXVGA1_RXVGA2,
    BLADERF_LB_BB_TXLPF_RXLPF,
    BLADERF_LB_BB_TXVGA1_RXLPF,
    BLADERF_LB_RF_LNA1,
    BLADERF_LB_RF_LNA2,
    BLADERF_LB_RF_LNA3,
    BLADERF_LB_NONE,
    BLADERF_LB_AD9361_BIST,
} bladerf_loopback;

int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback lb);
int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *lb);

typedef enum  {
    BLADERF_TRIGGER_ROLE_INVALID = -1,
    BLADERF_TRIGGER_ROLE_DISABLED,
    BLADERF_TRIGGER_ROLE_MASTER,
    BLADERF_TRIGGER_ROLE_SLAVE,
} bladerf_trigger_role;

typedef enum  {
    BLADERF_TRIGGER_INVALID = -1, /**< Invalid selection */
    BLADERF_TRIGGER_J71_4,        /**< J71 pin 4 (mini_exp_1) */

    BLADERF_TRIGGER_USER_0 = 128, /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_1,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_2,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_3,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_4,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_5,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_6,       /**< Reserved for user SW/HW customizations */
    BLADERF_TRIGGER_USER_7,       /**< Reserved for user SW/HW customizations */
} bladerf_trigger_signal;

struct bladerf_trigger
{
    bladerf_channel channel;       /**< RX/TX channel associated with trigger */
    bladerf_trigger_role role;     /**< Role of the device in a trigger chain */
    bladerf_trigger_signal signal; /**< Pin or signal being used */
    uint64_t options;              /**< Reserved field for future options. This
                                    *   is unused and should be set to 0.
                                    */
};

int bladerf_trigger_init(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, struct bladerf_trigger *trigger);
int bladerf_trigger_arm(struct bladerf *dev, const struct bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2);
int bladerf_trigger_fire(struct bladerf *dev, const struct bladerf_trigger *trigger);
int bladerf_trigger_state(struct bladerf *dev, const struct bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool *fire_requested, uint64_t *resv1, uint64_t *resv2);

typedef enum {
    BLADERF_RX_MUX_INVALID = -1,
    BLADERF_RX_MUX_BASEBAND = 0x0,
    BLADERF_RX_MUX_12BIT_COUNTER = 0x1,
    BLADERF_RX_MUX_32BIT_COUNTER = 0x2,
    BLADERF_RX_MUX_DIGITAL_LOOPBACK = 0x4,
} bladerf_rx_mux;

int bladerf_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mux);
int bladerf_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode);

#define BLADERF_RETUNE_NOW  0

struct bladerf_quick_tune {
   union {
        struct {
            uint8_t freqsel;
            uint8_t vcocap;
            uint16_t nint;
            uint32_t nfrac;
            uint8_t  flags;
        };
   };
};

int bladerf_schedule_retune(struct bladerf *dev, bladerf_channel ch, uint64_t timestamp, uint64_t frequency, struct bladerf_quick_tune *quick_tune);
int bladerf_cancel_scheduled_retunes(struct bladerf *dev, bladerf_channel ch);
int bladerf_get_quick_tune(struct bladerf *dev, bladerf_channel ch, struct bladerf_quick_tune *quick_tune);

typedef enum {
    BLADERF_CORR_DCOFF_I,
    BLADERF_CORR_DCOFF_Q,
    BLADERF_CORR_PHASE,
    BLADERF_CORR_GAIN
} bladerf_correction;

int bladerf_set_correction(struct bladerf *dev, bladerf_channel ch, bladerf_correction corr, int16_t value);
int bladerf_get_correction(struct bladerf *dev, bladerf_channel ch, bladerf_correction corr, int16_t *value);

typedef enum {
    BLADERF_RX = 0,                   /**< Receive direction */
    BLADERF_TX = 1,                   /**< Transmit direction */
} bladerf_direction;

typedef enum {
    BLADERF_RX_X1  = 0,    /**< x1 RX (SISO) */
    BLADERF_TX_X1  = 1,    /**< x1 TX (SISO) */
    BLADERF_RX_X2  = 2,    /**< x2 RX (MIMO) */
    BLADERF_TX_X2  = 3,    /**< x2 TX (MIMO) */
} bladerf_channel_layout;

#define BLADERF_DIRECTION_MASK 0x1

typedef enum {
    BLADERF_FORMAT_SC16_Q11,
    BLADERF_FORMAT_SC16_Q11_META,
} bladerf_format;

#define BLADERF_META_STATUS_OVERRUN  0x1
#define BLADERF_META_STATUS_UNDERRUN 0x2
#define BLADERF_META_FLAG_TX_BURST_START   0x1
#define BLADERF_META_FLAG_TX_BURST_END     0x2
#define BLADERF_META_FLAG_TX_NOW           0x4
#define BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP 0x8
#define BLADERF_META_FLAG_RX_NOW           0x80000000

struct bladerf_metadata {
    uint64_t timestamp;
    uint32_t flags;
    uint32_t status;
    unsigned int actual_count;
    uint8_t reserved[32];
};

int bladerf_enable_module(struct bladerf *dev, bladerf_direction dir, bool enable);
int bladerf_get_timestamp(struct bladerf *dev, bladerf_direction dir, uint64_t *value);
int bladerf_sync_config(struct bladerf *dev, bladerf_channel_layout layout, bladerf_format format, unsigned int num_buffers, unsigned int buffer_size, unsigned int num_transfers, unsigned int stream_timeout);
int bladerf_sync_tx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms);
int bladerf_sync_rx(struct bladerf *dev, void *samples, unsigned int num_samples, struct bladerf_metadata *metadata, unsigned int timeout_ms);

#define BLADERF_STREAM_SHUTDOWN 0
#define BLADERF_STREAM_NO_DATA  -1

struct bladerf_stream;

typedef void *(*bladerf_stream_cb)(struct bladerf *dev,
                                   struct bladerf_stream *stream,
                                   struct bladerf_metadata *meta,
                                   void *samples,
                                   size_t num_samples,
                                   void *user_data);

int bladerf_init_stream(struct bladerf_stream **stream, struct bladerf *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers, bladerf_format format, size_t samples_per_buffer, size_t num_transfers, void *user_data);
int bladerf_stream(struct bladerf_stream *stream, bladerf_channel_layout layout);
int bladerf_submit_stream_buffer(struct bladerf_stream *stream, void *buffer, unsigned int timeout_ms);
int bladerf_submit_stream_buffer_nb(struct bladerf_stream *stream, void *buffer);
void bladerf_deinit_stream(struct bladerf_stream *stream);

int bladerf_set_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int timeout);
int bladerf_get_stream_timeout(struct bladerf *dev, bladerf_direction dir, unsigned int *timeout);

int bladerf_flash_firmware(struct bladerf *dev, const char *firmware);
int bladerf_load_fpga(struct bladerf *dev, const char *fpga);
int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_image);
int bladerf_erase_stored_fpga(struct bladerf *dev);
int bladerf_device_reset(struct bladerf *dev);

int bladerf_get_fw_log(struct bladerf *dev, const char *filename);
int bladerf_jump_to_bootloader(struct bladerf *dev);
int bladerf_get_bootloader_list(struct bladerf_devinfo **list);
int bladerf_load_fw_from_bootloader(const char *device_identifier, bladerf_backend backend, uint8_t bus, uint8_t addr, const char *file);

typedef enum {
    BLADERF_IMAGE_TYPE_INVALID = -1,  /**< Used to denote invalid value */
    BLADERF_IMAGE_TYPE_RAW,           /**< Misc. raw data */
    BLADERF_IMAGE_TYPE_FIRMWARE,      /**< Firmware data */
    BLADERF_IMAGE_TYPE_FPGA_40KLE,    /**< FPGA bitstream for 40 KLE device */
    BLADERF_IMAGE_TYPE_FPGA_115KLE,   /**< FPGA bitstream for 115  KLE device */
    BLADERF_IMAGE_TYPE_CALIBRATION,   /**< Board calibration */
    BLADERF_IMAGE_TYPE_RX_DC_CAL,     /**< RX DC offset calibration table */
    BLADERF_IMAGE_TYPE_TX_DC_CAL,     /**< TX DC offset calibration table */
    BLADERF_IMAGE_TYPE_RX_IQ_CAL,     /**< RX IQ balance calibration table */
    BLADERF_IMAGE_TYPE_TX_IQ_CAL,     /**< TX IQ balance calibration table */
} bladerf_image_type;

#define BLADERF_IMAGE_MAGIC_LEN 7
#define BLADERF_IMAGE_CHECKSUM_LEN 32
#define BLADERF_IMAGE_RESERVED_LEN 128

struct bladerf_image {
    char magic[BLADERF_IMAGE_MAGIC_LEN + 1];
    uint8_t checksum[BLADERF_IMAGE_CHECKSUM_LEN];
    struct bladerf_version version;
    uint64_t timestamp;
    char serial[BLADERF_SERIAL_LENGTH + 1];
    char reserved[BLADERF_IMAGE_RESERVED_LEN];
    bladerf_image_type type;
    uint32_t address;
    uint32_t length;
    uint8_t *data;
};

struct bladerf_image * bladerf_alloc_image(bladerf_image_type type, uint32_t address, uint32_t length);
struct bladerf_image * bladerf_alloc_cal_image(bladerf_fpga_size fpga_size, uint16_t vctcxo_trim);
void bladerf_free_image(struct bladerf_image *image);
int bladerf_image_write(struct bladerf_image *image, const char *file);
int bladerf_image_read(struct bladerf_image *image, const char *file);

typedef enum {
    BLADERF_VCTCXO_TAMER_INVALID = -1,
    BLADERF_VCTCXO_TAMER_DISABLED = 0,
    BLADERF_VCTCXO_TAMER_1_PPS = 1,
    BLADERF_VCTCXO_TAMER_10_MHZ = 2
} bladerf_vctcxo_tamer_mode;

int bladerf_set_vctcxo_tamer_mode(struct bladerf *dev, bladerf_vctcxo_tamer_mode mode);
int bladerf_get_vctcxo_tamer_mode(struct bladerf *dev, bladerf_vctcxo_tamer_mode *mode);

int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);
int bladerf_trim_dac_write(struct bladerf *dev, uint16_t val);
int bladerf_trim_dac_read(struct bladerf *dev, uint16_t *val);

typedef enum {
    BLADERF_TUNING_MODE_INVALID = -1,
    BLADERF_TUNING_MODE_HOST,
    BLADERF_TUNING_MODE_FPGA,
} bladerf_tuning_mode;

int bladerf_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode mode);
int bladerf_get_tuning_mode(struct bladerf *dev, bladerf_tuning_mode *mode);

#define BLADERF_TRIGGER_REG_ARM     0x1
#define BLADERF_TRIGGER_REG_FIRE    0x2
#define BLADERF_TRIGGER_REG_MASTER  0x4
#define BLADERF_TRIGGER_REG_LINE    0x8

int bladerf_read_trigger(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, uint8_t *val);
int bladerf_write_trigger(struct bladerf *dev, bladerf_channel ch, bladerf_trigger_signal signal, uint8_t val);

int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);
int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);

int bladerf_erase_flash(struct bladerf *dev, uint32_t erase_block, uint32_t count);
int bladerf_read_flash(struct bladerf *dev, uint8_t *buf, uint32_t page, uint32_t count);
int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf, uint32_t page, uint32_t count);

typedef enum {
    BLADERF_XB_NONE = 0,    /**< No expansion boards attached */
    BLADERF_XB_100,         /**< XB-100 GPIO expansion board.
                             *   This device is not yet supported in
                             *   libbladeRF, and is here as a placeholder
                             *   for future support. */
    BLADERF_XB_200,         /**< XB-200 Transverter board */
    BLADERF_XB_300          /**< XB-300 Amplifier board */
} bladerf_xb;

int bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb);
int bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb *xb);

typedef enum {
    BLADERF_LOG_LEVEL_VERBOSE,  /**< Verbose level logging */
    BLADERF_LOG_LEVEL_DEBUG,    /**< Debug level logging */
    BLADERF_LOG_LEVEL_INFO,     /**< Information level logging */
    BLADERF_LOG_LEVEL_WARNING,  /**< Warning level logging */
    BLADERF_LOG_LEVEL_ERROR,    /**< Error level logging */
    BLADERF_LOG_LEVEL_CRITICAL, /**< Fatal error level logging */
    BLADERF_LOG_LEVEL_SILENT    /**< No output */
} bladerf_log_level;

void bladerf_log_set_verbosity(bladerf_log_level level);

void bladerf_version(struct bladerf_version *version);

#define BLADERF_ERR_UNEXPECTED  -1  /**< An unexpected failure occurred */
#define BLADERF_ERR_RANGE       -2  /**< Provided parameter is out of range */
#define BLADERF_ERR_INVAL       -3  /**< Invalid operation/parameter */
#define BLADERF_ERR_MEM         -4  /**< Memory allocation error */
#define BLADERF_ERR_IO          -5  /**< File/Device I/O error */
#define BLADERF_ERR_TIMEOUT     -6  /**< Operation timed out */
#define BLADERF_ERR_NODEV       -7  /**< No device(s) available */
#define BLADERF_ERR_UNSUPPORTED -8  /**< Operation not supported */
#define BLADERF_ERR_MISALIGNED  -9  /**< Misaligned flash access */
#define BLADERF_ERR_CHECKSUM    -10 /**< Invalid checksum */
#define BLADERF_ERR_NO_FILE     -11 /**< File not found */
#define BLADERF_ERR_UPDATE_FPGA -12 /**< An FPGA update is required */
#define BLADERF_ERR_UPDATE_FW   -13 /**< A firmware update is requied */
#define BLADERF_ERR_TIME_PAST   -14 /**< Requested timestamp is in the past */
#define BLADERF_ERR_QUEUE_FULL  -15 /**< Could not enqueue data into
                                     *   full queue */
#define BLADERF_ERR_FPGA_OP     -16 /**< An FPGA operation reported failure */
#define BLADERF_ERR_PERMISSION  -17 /**< Insufficient permissions for the
                                     *   requested operation */
#define BLADERF_ERR_WOULD_BLOCK -18 /**< Operation would block, but has been
                                       *   requested to be non-blocking. This
                                       *   indicates to a caller that it may
                                       *   need to retry the operation later.
                                       */
#define BLADERF_ERR_NOT_INIT    -19 /**< Device insufficiently initialized
                                       *   for operation */

const char * bladerf_strerror(int error);
""")

libbladeRF = ffi.dlopen("libbladeRF.so")

################################################################################

# Python class wrappers for various structures

class Version(collections.namedtuple("Version", ["major", "minor", "patch", "describe"])):
    @staticmethod
    def from_struct(version):
        return Version(version.major, version.minor, version.patch, ffi.string(version.describe).decode())

    def __str__(self):
        return "v{}.{}.{} (\"{}\")".format(*self)


class RationalRate(collections.namedtuple("RationalRate", ["integer", "num", "den"])):
    @staticmethod
    def from_struct(rate):
        return RationalRate(rate.integer, rate.num, rate.den)

    def to_struct(self):
        return ffi.new("struct bladerf_rational_rate *", [self.integer, self.num, self.den])


class DevInfo(collections.namedtuple("DevInfo", ["backend", "serial", "usb_bus", "usb_addr", "instance"])):
    @staticmethod
    def from_struct(devinfo):
        return DevInfo(ffi.string(libbladeRF.bladerf_backend_str(devinfo.backend)).decode(),
                       ffi.string(devinfo.serial).decode(),
                       devinfo.usb_bus,
                       devinfo.usb_addr,
                       devinfo.instance)

    def __str__(self):
        s =  "Device Information\n"
        s += "    backend  {}\n".format(self.backend)
        s += "    serial   {}\n".format(self.serial)
        s += "    usb_bus  {}\n".format(self.usb_bus)
        s += "    usb_addr {}\n".format(self.usb_addr)
        s += "    instance {}".format(self.instance)
        return s


class Range(collections.namedtuple("Range", ["min", "max", "step", "scale"])):
    @staticmethod
    def from_struct(_range):
        return Range(_range.min, _range.max, _range.step, _range.scale)

    def __str__(self):
        s =  "Range\n"
        s += "    min   {}\n".format(self.min)
        s += "    max   {}\n".format(self.max)
        s += "    step  {}\n".format(self.step)
        s += "    scale {}\n".format(self.scale)
        return s


class DeviceSpeed(enum.Enum):
    Unknown = libbladeRF.BLADERF_DEVICE_SPEED_UNKNOWN
    High = libbladeRF.BLADERF_DEVICE_SPEED_HIGH
    Super = libbladeRF.BLADERF_DEVICE_SPEED_SUPER


class Direction(enum.Enum):
    TX = libbladeRF.BLADERF_TX
    RX = libbladeRF.BLADERF_RX


class ChannelLayout(enum.Enum):
    RX_X1 = libbladeRF.BLADERF_RX_X1
    TX_X1 = libbladeRF.BLADERF_TX_X1
    RX_X2 = libbladeRF.BLADERF_RX_X2
    TX_X2 = libbladeRF.BLADERF_TX_X2


class Correction(enum.Enum):
    DCOFF_I = libbladeRF.BLADERF_CORR_DCOFF_I
    DCOFF_Q = libbladeRF.BLADERF_CORR_DCOFF_Q
    PHASE   = libbladeRF.BLADERF_CORR_PHASE
    GAIN    = libbladeRF.BLADERF_CORR_GAIN


class Format(enum.Enum):
    SC16_Q11 = libbladeRF.BLADERF_FORMAT_SC16_Q11
    SC16_Q11_META = libbladeRF.BLADERF_FORMAT_SC16_Q11_META


################################################################################

class BladeRFError(Exception):
    def __init__(self, code):
        self.code = code
        self.errstr = ffi.string(libbladeRF.bladerf_strerror(code)).decode()

    def __str__(self):
        return self.errstr


def _check_error(err):
    if err < 0:
        raise BladeRFError(err)

################################################################################

def version():
    version = ffi.new("struct bladerf_version *")
    libbladeRF.bladerf_version(version)
    return Version.from_struct(version)


def get_device_list():
    devices = ffi.new("struct bladerf_devinfo *[1]")
    ret = libbladeRF.bladerf_get_device_list(devices)
    _check_error(ret)
    devinfos = [DevInfo.from_struct(devices[0][i]) for i in range(ret)]
    libbladeRF.bladerf_free_device_list(devices[0])
    return devinfos

################################################################################

RX = 0x0

TX = 0x1

def CHANNEL_RX(ch): return (ch << 1)

def CHANNEL_TX(ch): return (ch << 1) | 0x1

################################################################################

class BladeRF:
    def __init__(self, device_identifier=None):
        self.dev = ffi.new("struct bladerf *[1]")
        self.open(device_identifier)

    # Open, close, devinfo

    def open(self, device_identifier=None):
        device_identifier = device_identifier.encode() if device_identifier else ffi.NULL
        ret = libbladeRF.bladerf_open(self.dev, device_identifier)
        _check_error(ret)

    def close(self):
        libbladeRF.bladerf_close(self.dev[0])

    def get_devinfo(self):
        devinfo = ffi.new("struct bladerf_devinfo *")
        ret = libbladeRF.bladerf_get_devinfo(self.dev[0], devinfo)
        _check_error(ret)
        return DevInfo.from_struct(devinfo)

    # Device properties

    def get_device_speed(self):
        return DeviceSpeed(libbladeRF.bladerf_device_speed(self.dev[0]))

    def get_serial(self):
        serial = ffi.new("char []", libbladeRF.BLADERF_SERIAL_LENGTH)
        ret = libbladeRF.bladerf_get_serial(self.dev[0], serial)
        _check_error(ret)
        return ffi.string(serial).decode()

    def get_fpga_size(self):
        fpga_size = ffi.new("bladerf_fpga_size *")
        ret = libbladeRF.bladerf_get_fpga_size(self.dev[0], fpga_size)
        _check_error(ret)
        return fpga_size[0]

    def is_fpga_configured(self):
        ret = libbladeRF.bladerf_is_fpga_configured(self.dev[0])
        _check_error(ret)
        return bool(ret)

    def get_fpga_version(self):
        version = ffi.new("struct bladerf_version *")
        ret = libbladeRF.bladerf_fpga_version(self.dev[0], version)
        _check_error(ret)
        return Version.from_struct(version)

    def get_fw_version(self):
        version = ffi.new("struct bladerf_version *")
        ret = libbladeRF.bladerf_fw_version(self.dev[0], version)
        _check_error(ret)
        return Version.from_struct(version)

    def get_board_name(self):
        board_name = libbladeRF.bladerf_get_board_name(self.dev[0])
        return ffi.string(board_name).decode()

    # Enable/Disable

    def enable_module(self, direction, enable):
        ret = libbladeRF.bladerf_enable_module(self.dev[0], direction, bool(enable))
        _check_error(ret)

    # Gain

    def set_gain(self, ch, gain):
        ret = libbladeRF.bladerf_set_gain(self.dev[0], ch, gain)
        _check_error(ret)

    def get_gain(self, ch):
        gain = ffi.new("int *")
        ret = libbladeRF.bladerf_get_gain(self.dev[0], ch, gain)
        _check_error(ret)
        return gain[0]

    def set_gain_mode(self, ch, mode):
        ret = libbladeRF.bladerf_set_gain_mode(self.dev[0], ch, mode)
        _check_error(ret)

    def get_gain_mode(self, ch):
        mode = ffi.new("bladerf_gain_mode *")
        ret = libbladeRF.bladerf_get_gain_mode(self.dev[0], ch, mode)
        _check_error(ret)
        return mode[0]

    def get_gain_range(self, ch):
        gain_range = ffi.new("struct bladerf_range *")
        ret = libbladeRF.bladerf_get_gain_range(self.dev[0], ch, gain_range)
        _check_error(ret)
        return Range.from_struct(gain_range)

    def set_gain_stage(self, ch, stage, gain):
        ret = libbladeRF.bladerf_set_gain_stage(self.dev[0], ch, stage.encode(), gain)
        _check_error(ret)

    def get_gain_stage(self, ch, stage):
        gain = ffi.new("int *")
        ret = libbladeRF.bladerf_get_gain_stage(self.dev[0], ch, stage.encode(), gain)
        _check_error(ret)
        return gain[0]

    def get_gain_stage_range(self, ch, stage):
        gain_range = ffi.new("struct bladerf_range *")
        ret = libbladeRF.bladerf_get_gain_stage_range(self.dev[0], ch, stage.encode(), gain_range)
        _check_error(ret)
        return Range.from_struct(gain_range)

    def get_gain_stages(self, ch):
        ret = libbladeRF.bladerf_get_gain_stages(self.dev[0], ch, ffi.NULL, 0)
        _check_error(ret)
        stages_arr = ffi.new("const char *[]", ret)
        ret = libbladeRF.bladerf_get_gain_stages(self.dev[0], ch, stages_arr, ret)
        _check_error(ret)
        stages = [ffi.string(stages_arr[i]).decode() for i in range(ret)]
        return stages

    # Sample rate

    def set_sample_rate(self, ch, rate):
        actual_rate = ffi.new("unsigned int *")
        ret = libbladeRF.bladerf_set_sample_rate(self.dev[0], ch, int(rate), actual_rate)
        _check_error(ret)
        return actual_rate[0]

    def set_rational_sample_rate(self, ch, rational_rate):
        raise NotImplementedError()

    def get_sample_rate(self, ch):
        rate = ffi.new("unsigned int *")
        ret = libbladeRF.bladerf_get_sample_rate(self.dev[0], ch, rate)
        _check_error(ret)
        return rate[0]

    def get_rational_sample_rate(self, ch):
        raise NotImplementedError()

    def get_sample_rate_range(self, ch):
        sample_rate_range = ffi.new("struct bladerf_range *")
        ret = libbladeRF.bladerf_get_sample_rate_range(self.dev[0], ch, sample_rate_range)
        _check_error(ret)
        return Range.from_struct(sample_rate_range)

    # Bandwidth

    def set_bandwidth(self, ch, bandwidth):
        actual_bandwidth = ffi.new("unsigned int *")
        ret = libbladeRF.bladerf_set_bandwidth(self.dev[0], ch, int(bandwidth), actual_bandwidth)
        _check_error(ret)
        return actual_bandwidth[0]

    def get_bandwidth(self, ch):
        bandwidth = ffi.new("unsigned int *")
        ret = libbladeRF.bladerf_get_bandwidth(self.dev[0], ch, bandwidth)
        _check_error(ret)
        return bandwidth[0]

    def get_bandwidth_range(self, ch):
        bandwidth_range = ffi.new("struct bladerf_range *")
        ret = libbladeRF.bladerf_get_bandwidth_range(self.dev[0], ch, bandwidth_range)
        _check_error(ret)
        return Range.from_struct(bandwidth_range)

    # Frequency

    def set_frequency(self, ch, frequency):
        ret = libbladeRF.bladerf_set_frequency(self.dev[0], ch, int(frequency))
        _check_error(ret)

    def get_frequency(self, ch):
        frequency = ffi.new("uint64_t *")
        ret = libbladeRF.bladerf_get_frequency(self.dev[0], ch, frequency)
        _check_error(ret)
        return frequency[0]

    def select_band(self, ch, frequency):
        ret = libbladeRF.bladerf_select_band(self.dev[0], ch, int(frequency))
        _check_error(ret)

    def get_frequency_range(self, ch):
        frequency_range = ffi.new("struct bladerf_range *")
        ret = libbladeRF.bladerf_get_frequency_range(self.dev[0], ch, frequency_range)
        _check_error(ret)
        return Range.from_struct(frequency_range)

    # RF Ports

    def set_rf_port(self, ch, port):
        ret = libbladeRF.bladerf_set_rf_port(self.dev[0], ch, port.encode())
        _check_error(ret)

    def get_rf_port(self, ch):
        port = ffi.new("const char *[1]")
        ret = libbladeRF.bladerf_get_rf_port(self.dev[0], ch, port)
        _check_error(ret)
        return ffi.string(port[0]).decode()

    def get_rf_ports(self, ch):
        ret = libbladeRF.bladerf_get_rf_ports(self.dev[0], ch, ffi.NULL, 0)
        _check_error(ret)
        ports_arr = ffi.new("const char *[]", ret)
        ret = libbladeRF.bladerf_get_rf_ports(self.dev[0], ch, ports_arr, ret)
        _check_error(ret)
        ports = [ffi.string(ports_arr[i]).decode() for i in range(ret)]
        return ports

    # Sample RX FPGA Mux

    def set_loopback(self, lb):
        raise NotImplementedError()

    def get_loopback(self):
        raise NotImplementedError()

    # Trigger TBD

    # Sample RX Mux

    def set_rx_mux(self, rx_mux):
        raise NotImplementedError()

    def get_rx_mux(self):
        raise NotImplementedError()

    # DC/Phase/Gain Correction

    def get_correction(self, ch, corr):
        value = ffi.new("int16_t *")
        ret = libbladeRF.bladerf_get_correction(self.dev[0], ch, corr.value, value)
        _check_error(ret)
        return value[0]

    def set_correction(self, ch, corr, value):
        ret = libbladeRF.bladerf_set_correction(self.dev[0], ch, corr.value, value)
        _check_error(ret)

    # Streaming

    def sync_config(self, layout, fmt, num_buffers, buffer_size, num_transfers, stream_timeout):
        if fmt != Format.SC16_Q11:
            raise NotImplementedError("Format not supported by binding.")

        ret = libbladeRF.bladerf_sync_config(self.dev[0], layout, fmt.value, num_buffers, buffer_size, num_transfers, stream_timeout)
        _check_error(ret)

    def sync_tx(self, buf, num_samples, timeout_ms=None):
        ret = libbladeRF.bladerf_sync_tx(self.dev[0], ffi.from_buffer(buf), num_samples, ffi.NULL, timeout_ms or 0)
        _check_error(ret)

    def sync_rx(self, buf, num_samples, timeout_ms=None):
        ret = libbladeRF.bladerf_sync_rx(self.dev[0], ffi.from_buffer(buf), num_samples, ffi.NULL, timeout_ms or 0)
        _check_error(ret)

    # FPGA/Firmware Loading/Flashing

    def load_fpga(self, image_path):
        ret = libbladeRF.bladerf_load_fpga(self.dev[0], image_path.encode())
        _check_error(ret)

    def flash_fpga(self, image_path):
        ret = libbladeRF.bladerf_flash_fpga(self.dev[0], image_path.encode())
        _check_error(ret)

    def erase_stored_fpga(self):
        ret = libbladeRF.bladerf_erase_stored_fpga(self.dev[0])
        _check_error(ret)

    def flash_firmware(self, image_path):
        ret = libbladeRF.bladerf_flash_firmware(self.dev[0], image_path.encode())
        _check_error(ret)

    def device_reset(self):
        ret = libbladeRF.bladerf_device_reset(self.dev[0], image_path.encode())
        _check_error(ret)

    # VCTCXO Tamer Mode

    def set_vctcxo_tamer_mode(self, mode):
        raise NotImplementedError()

    def get_vctcxo_tamer_mode(self):
        raise NotImplementedError()

    def get_vctcxo_trim(self):
        raise NotImplementedError()

    # Tuning Mode

    def set_tuning_mode(self, mode):
        raise NotImplementedError()

    def get_tuning_mode(self):
        raise NotImplementedError()

