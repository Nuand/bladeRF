# Generated from libbladeRF.h by import_header.py

header = """
  struct bladerf;
  typedef enum
  {
    BLADERF_BACKEND_ANY,
    BLADERF_BACKEND_LINUX,
    BLADERF_BACKEND_LIBUSB,
    BLADERF_BACKEND_CYPRESS,
    BLADERF_BACKEND_DUMMY = 100
  } bladerf_backend;
  struct bladerf_devinfo
  {
    bladerf_backend backend;
    char serial[33];
    uint8_t usb_bus;
    uint8_t usb_addr;
    unsigned int instance;
    char manufacturer[33];
    char product[33];
  };
  struct bladerf_backendinfo
  {
    int handle_count;
    void *handle;
    int lock_count;
    void *lock;
  };
  int bladerf_open(struct bladerf **device, const char
    *device_identifier);
  void bladerf_close(struct bladerf *device);
  int bladerf_open_with_devinfo(struct bladerf **device, struct
    bladerf_devinfo *devinfo);
  int bladerf_get_device_list(struct bladerf_devinfo **devices);
  void bladerf_free_device_list(struct bladerf_devinfo *devices);
  void bladerf_init_devinfo(struct bladerf_devinfo *info);
  int bladerf_get_devinfo(struct bladerf *dev, struct bladerf_devinfo
    *info);
  int bladerf_get_backendinfo(struct bladerf *dev, struct
    bladerf_backendinfo *info);
  int bladerf_get_devinfo_from_str(const char *devstr, struct
    bladerf_devinfo *info);
  bool bladerf_devinfo_matches(const struct bladerf_devinfo *a, const
    struct bladerf_devinfo *b);
  bool bladerf_devstr_matches(const char *dev_str, struct
    bladerf_devinfo *info);
  const char *bladerf_backend_str(bladerf_backend backend);
  void bladerf_set_usb_reset_on_open(bool enabled);
  struct bladerf_range
  {
    int64_t min;
    int64_t max;
    int64_t step;
    float scale;
  };
  struct bladerf_serial
  {
    char serial[33];
  };
  struct bladerf_version
  {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    const char *describe;
  };
  typedef enum
  {
    BLADERF_FPGA_UNKNOWN = 0,
    BLADERF_FPGA_40KLE = 40,
    BLADERF_FPGA_115KLE = 115,
    BLADERF_FPGA_A4 = 49,
    BLADERF_FPGA_A5 = 77,
    BLADERF_FPGA_A9 = 301
  } bladerf_fpga_size;
  typedef enum
  {
    BLADERF_DEVICE_SPEED_UNKNOWN,
    BLADERF_DEVICE_SPEED_HIGH,
    BLADERF_DEVICE_SPEED_SUPER
  } bladerf_dev_speed;
  typedef enum
  {
    BLADERF_FPGA_SOURCE_UNKNOWN = 0,
    BLADERF_FPGA_SOURCE_FLASH = 1,
    BLADERF_FPGA_SOURCE_HOST = 2
  } bladerf_fpga_source;
  int bladerf_get_serial(struct bladerf *dev, char *serial);
  int bladerf_get_serial_struct(struct bladerf *dev, struct
    bladerf_serial *serial);
  int bladerf_get_fpga_size(struct bladerf *dev, bladerf_fpga_size
    *size);
  int bladerf_get_fpga_bytes(struct bladerf *dev, size_t *size);
  int bladerf_get_flash_size(struct bladerf *dev, uint32_t *size, bool
    *is_guess);
  int bladerf_fw_version(struct bladerf *dev, struct bladerf_version
    *version);
  int bladerf_is_fpga_configured(struct bladerf *dev);
  int bladerf_fpga_version(struct bladerf *dev, struct bladerf_version
    *version);
  int bladerf_get_fpga_source(struct bladerf *dev, bladerf_fpga_source
    *source);
  bladerf_dev_speed bladerf_device_speed(struct bladerf *dev);
  const char *bladerf_get_board_name(struct bladerf *dev);
  typedef int bladerf_channel;
  typedef bladerf_channel bladerf_module;
  typedef enum
  {
    BLADERF_RX = 0,
    BLADERF_TX = 1
  } bladerf_direction;
  typedef enum
  {
    BLADERF_RX_X1 = 0,
    BLADERF_TX_X1 = 1,
    BLADERF_RX_X2 = 2,
    BLADERF_TX_X2 = 3
  } bladerf_channel_layout;
  size_t bladerf_get_channel_count(struct bladerf *dev,
    bladerf_direction dir);
  typedef int bladerf_gain;
  typedef enum
  {
    BLADERF_GAIN_DEFAULT,
    BLADERF_GAIN_MGC,
    BLADERF_GAIN_FASTATTACK_AGC,
    BLADERF_GAIN_SLOWATTACK_AGC,
    BLADERF_GAIN_HYBRID_AGC
  } bladerf_gain_mode;
  struct bladerf_gain_modes
  {
    const char *name;
    bladerf_gain_mode mode;
  };
  int bladerf_set_gain(struct bladerf *dev, bladerf_channel ch,
    bladerf_gain gain);
  int bladerf_get_gain(struct bladerf *dev, bladerf_channel ch,
    bladerf_gain *gain);
  int bladerf_set_gain_mode(struct bladerf *dev, bladerf_channel ch,
    bladerf_gain_mode mode);
  int bladerf_get_gain_mode(struct bladerf *dev, bladerf_channel ch,
    bladerf_gain_mode *mode);
  int bladerf_get_gain_modes(struct bladerf *dev, bladerf_channel ch,
    const struct bladerf_gain_modes **modes);
  int bladerf_get_gain_range(struct bladerf *dev, bladerf_channel ch,
    const struct bladerf_range **range);
  int bladerf_set_gain_stage(struct bladerf *dev, bladerf_channel ch,
    const char *stage, bladerf_gain gain);
  int bladerf_get_gain_stage(struct bladerf *dev, bladerf_channel ch,
    const char *stage, bladerf_gain *gain);
  int bladerf_get_gain_stage_range(struct bladerf *dev, bladerf_channel
    ch, const char *stage, const struct bladerf_range **range);
  int bladerf_get_gain_stages(struct bladerf *dev, bladerf_channel ch,
    const char **stages, size_t count);
  typedef unsigned int bladerf_sample_rate;
  struct bladerf_rational_rate
  {
    uint64_t integer;
    uint64_t num;
    uint64_t den;
  };
  int bladerf_set_sample_rate(struct bladerf *dev, bladerf_channel ch,
    bladerf_sample_rate rate, bladerf_sample_rate *actual);
  int bladerf_set_rational_sample_rate(struct bladerf *dev,
    bladerf_channel ch, struct bladerf_rational_rate *rate, struct
    bladerf_rational_rate *actual);
  int bladerf_get_sample_rate(struct bladerf *dev, bladerf_channel ch,
    bladerf_sample_rate *rate);
  int bladerf_get_sample_rate_range(struct bladerf *dev, bladerf_channel
    ch, const struct bladerf_range **range);
  int bladerf_get_rational_sample_rate(struct bladerf *dev,
    bladerf_channel ch, struct bladerf_rational_rate *rate);
  typedef unsigned int bladerf_bandwidth;
  int bladerf_set_bandwidth(struct bladerf *dev, bladerf_channel ch,
    bladerf_bandwidth bandwidth, bladerf_bandwidth *actual);
  int bladerf_get_bandwidth(struct bladerf *dev, bladerf_channel ch,
    bladerf_bandwidth *bandwidth);
  int bladerf_get_bandwidth_range(struct bladerf *dev, bladerf_channel
    ch, const struct bladerf_range **range);
  typedef uint64_t bladerf_frequency;
  int bladerf_select_band(struct bladerf *dev, bladerf_channel ch,
    bladerf_frequency frequency);
  int bladerf_set_frequency(struct bladerf *dev, bladerf_channel ch,
    bladerf_frequency frequency);
  int bladerf_get_frequency(struct bladerf *dev, bladerf_channel ch,
    bladerf_frequency *frequency);
  int bladerf_get_frequency_range(struct bladerf *dev, bladerf_channel
    ch, const struct bladerf_range **range);
  typedef enum
  {
    BLADERF_LB_NONE = 0,
    BLADERF_LB_FIRMWARE,
    BLADERF_LB_BB_TXLPF_RXVGA2,
    BLADERF_LB_BB_TXVGA1_RXVGA2,
    BLADERF_LB_BB_TXLPF_RXLPF,
    BLADERF_LB_BB_TXVGA1_RXLPF,
    BLADERF_LB_RF_LNA1,
    BLADERF_LB_RF_LNA2,
    BLADERF_LB_RF_LNA3,
    BLADERF_LB_RFIC_BIST
  } bladerf_loopback;
  struct bladerf_loopback_modes
  {
    const char *name;
    bladerf_loopback mode;
  };
  int bladerf_get_loopback_modes(struct bladerf *dev, const struct
    bladerf_loopback_modes **modes);
  bool bladerf_is_loopback_mode_supported(struct bladerf *dev,
    bladerf_loopback mode);
  int bladerf_set_loopback(struct bladerf *dev, bladerf_loopback lb);
  int bladerf_get_loopback(struct bladerf *dev, bladerf_loopback *lb);
  typedef enum
  {
    BLADERF_TRIGGER_ROLE_INVALID = -1,
    BLADERF_TRIGGER_ROLE_DISABLED,
    BLADERF_TRIGGER_ROLE_MASTER,
    BLADERF_TRIGGER_ROLE_SLAVE
  } bladerf_trigger_role;
  typedef enum
  {
    BLADERF_TRIGGER_INVALID = -1,
    BLADERF_TRIGGER_J71_4,
    BLADERF_TRIGGER_J51_1,
    BLADERF_TRIGGER_MINI_EXP_1,
    BLADERF_TRIGGER_USER_0 = 128,
    BLADERF_TRIGGER_USER_1,
    BLADERF_TRIGGER_USER_2,
    BLADERF_TRIGGER_USER_3,
    BLADERF_TRIGGER_USER_4,
    BLADERF_TRIGGER_USER_5,
    BLADERF_TRIGGER_USER_6,
    BLADERF_TRIGGER_USER_7
  } bladerf_trigger_signal;
  struct bladerf_trigger
  {
    bladerf_channel channel;
    bladerf_trigger_role role;
    bladerf_trigger_signal signal;
    uint64_t options;
  };
  int bladerf_trigger_init(struct bladerf *dev, bladerf_channel ch,
    bladerf_trigger_signal signal, struct bladerf_trigger *trigger);
  int bladerf_trigger_arm(struct bladerf *dev, const struct
    bladerf_trigger *trigger, bool arm, uint64_t resv1, uint64_t resv2);
  int bladerf_trigger_fire(struct bladerf *dev, const struct
    bladerf_trigger *trigger);
  int bladerf_trigger_state(struct bladerf *dev, const struct
    bladerf_trigger *trigger, bool *is_armed, bool *has_fired, bool
    *fire_requested, uint64_t *resv1, uint64_t *resv2);
  typedef enum
  {
    BLADERF_RX_MUX_INVALID = -1,
    BLADERF_RX_MUX_BASEBAND = 0x0,
    BLADERF_RX_MUX_12BIT_COUNTER = 0x1,
    BLADERF_RX_MUX_32BIT_COUNTER = 0x2,
    BLADERF_RX_MUX_DIGITAL_LOOPBACK = 0x4
  } bladerf_rx_mux;
  int bladerf_set_rx_mux(struct bladerf *dev, bladerf_rx_mux mux);
  int bladerf_get_rx_mux(struct bladerf *dev, bladerf_rx_mux *mode);
  typedef uint64_t bladerf_timestamp;
  struct bladerf_quick_tune
  {
    union
    {
      struct
      {
        uint8_t freqsel;
        uint8_t vcocap;
        uint16_t nint;
        uint32_t nfrac;
        uint8_t flags;
        uint8_t xb_gpio;
      };
      struct
      {
        uint16_t nios_profile;
        uint8_t rffe_profile;
        uint8_t port;
        uint8_t spdt;
      };
    };
  };
  int bladerf_schedule_retune(struct bladerf *dev, bladerf_channel ch,
    bladerf_timestamp timestamp, bladerf_frequency frequency, struct
    bladerf_quick_tune *quick_tune);
  int bladerf_cancel_scheduled_retunes(struct bladerf *dev,
    bladerf_channel ch);
  int bladerf_get_quick_tune(struct bladerf *dev, bladerf_channel ch,
    struct bladerf_quick_tune *quick_tune);
  typedef int16_t bladerf_correction_value;
  typedef enum
  {
    BLADERF_CORR_DCOFF_I,
    BLADERF_CORR_DCOFF_Q,
    BLADERF_CORR_PHASE,
    BLADERF_CORR_GAIN
  } bladerf_correction;
  int bladerf_set_correction(struct bladerf *dev, bladerf_channel ch,
    bladerf_correction corr, bladerf_correction_value value);
  int bladerf_get_correction(struct bladerf *dev, bladerf_channel ch,
    bladerf_correction corr, bladerf_correction_value *value);
  typedef enum
  {
    BLADERF_FORMAT_SC16_Q11,
    BLADERF_FORMAT_SC16_Q11_META,
    BLADERF_FORMAT_PACKET_META,
    BLADERF_FORMAT_SC8_Q7,
    BLADERF_FORMAT_SC8_Q7_META
  } bladerf_format;
  struct bladerf_metadata
  {
    bladerf_timestamp timestamp;
    uint32_t flags;
    uint32_t status;
    unsigned int actual_count;
    uint8_t reserved[32];
  };
  int bladerf_interleave_stream_buffer(bladerf_channel_layout layout,
    bladerf_format format, unsigned int buffer_size, void *samples);
  int bladerf_deinterleave_stream_buffer(bladerf_channel_layout layout,
    bladerf_format format, unsigned int buffer_size, void *samples);
  int bladerf_enable_module(struct bladerf *dev, bladerf_channel ch,
    bool enable);
  int bladerf_get_timestamp(struct bladerf *dev, bladerf_direction dir,
    bladerf_timestamp *timestamp);
  int bladerf_sync_config(struct bladerf *dev, bladerf_channel_layout
    layout, bladerf_format format, unsigned int num_buffers, unsigned int
    buffer_size, unsigned int num_transfers, unsigned int stream_timeout);
  int bladerf_sync_tx(struct bladerf *dev, const void *samples, unsigned
    int num_samples, struct bladerf_metadata *metadata, unsigned int
    timeout_ms);
  int bladerf_sync_rx(struct bladerf *dev, void *samples, unsigned int
    num_samples, struct bladerf_metadata *metadata, unsigned int
    timeout_ms);
  struct bladerf_stream;
  typedef void *(*bladerf_stream_cb)(struct bladerf *dev, struct
    bladerf_stream *stream, struct bladerf_metadata *meta, void *samples,
    size_t num_samples, void *user_data);
  int bladerf_init_stream(struct bladerf_stream **stream, struct bladerf
    *dev, bladerf_stream_cb callback, void ***buffers, size_t num_buffers,
    bladerf_format format, size_t samples_per_buffer, size_t
    num_transfers, void *user_data);
  int bladerf_stream(struct bladerf_stream *stream,
    bladerf_channel_layout layout);
  int bladerf_submit_stream_buffer(struct bladerf_stream *stream, void
    *buffer, unsigned int timeout_ms);
  int bladerf_submit_stream_buffer_nb(struct bladerf_stream *stream,
    void *buffer);
  void bladerf_deinit_stream(struct bladerf_stream *stream);
  int bladerf_set_stream_timeout(struct bladerf *dev, bladerf_direction
    dir, unsigned int timeout);
  int bladerf_get_stream_timeout(struct bladerf *dev, bladerf_direction
    dir, unsigned int *timeout);
  int bladerf_flash_firmware(struct bladerf *dev, const char *firmware);
  int bladerf_load_fpga(struct bladerf *dev, const char *fpga);
  int bladerf_flash_fpga(struct bladerf *dev, const char *fpga_image);
  int bladerf_erase_stored_fpga(struct bladerf *dev);
  int bladerf_device_reset(struct bladerf *dev);
  int bladerf_get_fw_log(struct bladerf *dev, const char *filename);
  int bladerf_jump_to_bootloader(struct bladerf *dev);
  int bladerf_get_bootloader_list(struct bladerf_devinfo **list);
  int bladerf_load_fw_from_bootloader(const char *device_identifier,
    bladerf_backend backend, uint8_t bus, uint8_t addr, const char *file);
  typedef enum
  {
    BLADERF_IMAGE_TYPE_INVALID = -1,
    BLADERF_IMAGE_TYPE_RAW,
    BLADERF_IMAGE_TYPE_FIRMWARE,
    BLADERF_IMAGE_TYPE_FPGA_40KLE,
    BLADERF_IMAGE_TYPE_FPGA_115KLE,
    BLADERF_IMAGE_TYPE_FPGA_A4,
    BLADERF_IMAGE_TYPE_FPGA_A9,
    BLADERF_IMAGE_TYPE_CALIBRATION,
    BLADERF_IMAGE_TYPE_RX_DC_CAL,
    BLADERF_IMAGE_TYPE_TX_DC_CAL,
    BLADERF_IMAGE_TYPE_RX_IQ_CAL,
    BLADERF_IMAGE_TYPE_TX_IQ_CAL,
    BLADERF_IMAGE_TYPE_FPGA_A5
  } bladerf_image_type;
  struct bladerf_image
  {
    char magic[8]; /* Original: char magic[7 + 1]; */
    uint8_t checksum[32];
    struct bladerf_version version;
    uint64_t timestamp;
    char serial[34]; /* Original: char serial[33 + 1]; */
    char reserved[128];
    bladerf_image_type type;
    uint32_t address;
    uint32_t length;
    uint8_t *data;
  };
  struct bladerf_image *bladerf_alloc_image(struct bladerf *dev,
    bladerf_image_type type, uint32_t address, uint32_t length);
  struct bladerf_image *bladerf_alloc_cal_image(struct bladerf *dev,
    bladerf_fpga_size fpga_size, uint16_t vctcxo_trim);
  void bladerf_free_image(struct bladerf_image *image);
  int bladerf_image_write(struct bladerf *dev, struct bladerf_image
    *image, const char *file);
  int bladerf_image_read(struct bladerf_image *image, const char *file);
  typedef enum
  {
    BLADERF_VCTCXO_TAMER_INVALID = -1,
    BLADERF_VCTCXO_TAMER_DISABLED = 0,
    BLADERF_VCTCXO_TAMER_1_PPS = 1,
    BLADERF_VCTCXO_TAMER_10_MHZ = 2
  } bladerf_vctcxo_tamer_mode;
  int bladerf_set_vctcxo_tamer_mode(struct bladerf *dev,
    bladerf_vctcxo_tamer_mode mode);
  int bladerf_get_vctcxo_tamer_mode(struct bladerf *dev,
    bladerf_vctcxo_tamer_mode *mode);
  int bladerf_get_vctcxo_trim(struct bladerf *dev, uint16_t *trim);
  int bladerf_trim_dac_write(struct bladerf *dev, uint16_t val);
  int bladerf_trim_dac_read(struct bladerf *dev, uint16_t *val);
  typedef enum
  {
    BLADERF_TUNING_MODE_INVALID = -1,
    BLADERF_TUNING_MODE_HOST,
    BLADERF_TUNING_MODE_FPGA
  } bladerf_tuning_mode;
  int bladerf_set_tuning_mode(struct bladerf *dev, bladerf_tuning_mode
    mode);
  int bladerf_get_tuning_mode(struct bladerf *dev, bladerf_tuning_mode
    *mode);
  int bladerf_read_trigger(struct bladerf *dev, bladerf_channel ch,
    bladerf_trigger_signal signal, uint8_t *val);
  int bladerf_write_trigger(struct bladerf *dev, bladerf_channel ch,
    bladerf_trigger_signal signal, uint8_t val);
  int bladerf_wishbone_master_read(struct bladerf *dev, uint32_t addr,
    uint32_t *data);
  int bladerf_wishbone_master_write(struct bladerf *dev, uint32_t addr,
    uint32_t val);
  int bladerf_config_gpio_read(struct bladerf *dev, uint32_t *val);
  int bladerf_config_gpio_write(struct bladerf *dev, uint32_t val);
  int bladerf_erase_flash(struct bladerf *dev, uint32_t erase_block,
    uint32_t count);
  int bladerf_erase_flash_bytes(struct bladerf *dev, uint32_t address,
    uint32_t length);
  int bladerf_read_flash(struct bladerf *dev, uint8_t *buf, uint32_t
    page, uint32_t count);
  int bladerf_read_flash_bytes(struct bladerf *dev, uint8_t *buf,
    uint32_t address, uint32_t bytes);
  int bladerf_write_flash(struct bladerf *dev, const uint8_t *buf,
    uint32_t page, uint32_t count);
  int bladerf_write_flash_bytes(struct bladerf *dev, const uint8_t *buf,
    uint32_t address, uint32_t length);
  int bladerf_lock_otp(struct bladerf *dev);
  int bladerf_read_otp(struct bladerf *dev, uint8_t *buf);
  int bladerf_write_otp(struct bladerf *dev, uint8_t *buf);
  int bladerf_set_rf_port(struct bladerf *dev, bladerf_channel ch, const
    char *port);
  int bladerf_get_rf_port(struct bladerf *dev, bladerf_channel ch, const
    char **port);
  int bladerf_get_rf_ports(struct bladerf *dev, bladerf_channel ch,
    const char **ports, unsigned int count);
  typedef enum
  {
    BLADERF_FEATURE_DEFAULT = 0,
    BLADERF_FEATURE_OVERSAMPLE
  } bladerf_feature;
  int bladerf_enable_feature(struct bladerf *dev, bladerf_feature
    feature, bool enable);
  int bladerf_get_feature(struct bladerf *dev, bladerf_feature
    *feature);
  typedef enum
  {
    BLADERF_XB_NONE = 0,
    BLADERF_XB_100,
    BLADERF_XB_200,
    BLADERF_XB_300
  } bladerf_xb;
  int bladerf_expansion_attach(struct bladerf *dev, bladerf_xb xb);
  int bladerf_expansion_get_attached(struct bladerf *dev, bladerf_xb
    *xb);
  typedef enum
  {
    BLADERF_LOG_LEVEL_VERBOSE,
    BLADERF_LOG_LEVEL_DEBUG,
    BLADERF_LOG_LEVEL_INFO,
    BLADERF_LOG_LEVEL_WARNING,
    BLADERF_LOG_LEVEL_ERROR,
    BLADERF_LOG_LEVEL_CRITICAL,
    BLADERF_LOG_LEVEL_SILENT
  } bladerf_log_level;
  void bladerf_log_set_verbosity(bladerf_log_level level);
  void bladerf_version(struct bladerf_version *version);
  const char *bladerf_strerror(int error);
  typedef enum
  {
    BLADERF_LNA_GAIN_UNKNOWN,
    BLADERF_LNA_GAIN_BYPASS,
    BLADERF_LNA_GAIN_MID,
    BLADERF_LNA_GAIN_MAX
  } bladerf_lna_gain;
  int bladerf_set_txvga2(struct bladerf *dev, int gain);
  int bladerf_get_txvga2(struct bladerf *dev, int *gain);
  int bladerf_set_txvga1(struct bladerf *dev, int gain);
  int bladerf_get_txvga1(struct bladerf *dev, int *gain);
  int bladerf_set_lna_gain(struct bladerf *dev, bladerf_lna_gain gain);
  int bladerf_get_lna_gain(struct bladerf *dev, bladerf_lna_gain *gain);
  int bladerf_set_rxvga1(struct bladerf *dev, int gain);
  int bladerf_get_rxvga1(struct bladerf *dev, int *gain);
  int bladerf_set_rxvga2(struct bladerf *dev, int gain);
  int bladerf_get_rxvga2(struct bladerf *dev, int *gain);
  typedef enum
  {
    BLADERF_SAMPLING_UNKNOWN,
    BLADERF_SAMPLING_INTERNAL,
    BLADERF_SAMPLING_EXTERNAL
  } bladerf_sampling;
  int bladerf_set_sampling(struct bladerf *dev, bladerf_sampling
    sampling);
  int bladerf_get_sampling(struct bladerf *dev, bladerf_sampling
    *sampling);
  typedef enum
  {
    BLADERF_LPF_NORMAL,
    BLADERF_LPF_BYPASSED,
    BLADERF_LPF_DISABLED
  } bladerf_lpf_mode;
  int bladerf_set_lpf_mode(struct bladerf *dev, bladerf_channel ch,
    bladerf_lpf_mode mode);
  int bladerf_get_lpf_mode(struct bladerf *dev, bladerf_channel ch,
    bladerf_lpf_mode *mode);
  typedef enum
  {
    BLADERF_SMB_MODE_INVALID = -1,
    BLADERF_SMB_MODE_DISABLED,
    BLADERF_SMB_MODE_OUTPUT,
    BLADERF_SMB_MODE_INPUT,
    BLADERF_SMB_MODE_UNAVAILBLE
  } bladerf_smb_mode;
  int bladerf_set_smb_mode(struct bladerf *dev, bladerf_smb_mode mode);
  int bladerf_get_smb_mode(struct bladerf *dev, bladerf_smb_mode *mode);
  int bladerf_set_rational_smb_frequency(struct bladerf *dev, struct
    bladerf_rational_rate *rate, struct bladerf_rational_rate *actual);
  int bladerf_set_smb_frequency(struct bladerf *dev, uint32_t rate,
    uint32_t *actual);
  int bladerf_get_rational_smb_frequency(struct bladerf *dev, struct
    bladerf_rational_rate *rate);
  int bladerf_get_smb_frequency(struct bladerf *dev, unsigned int
    *rate);
  int bladerf_expansion_gpio_read(struct bladerf *dev, uint32_t *val);
  int bladerf_expansion_gpio_write(struct bladerf *dev, uint32_t val);
  int bladerf_expansion_gpio_masked_write(struct bladerf *dev, uint32_t
    mask, uint32_t value);
  int bladerf_expansion_gpio_dir_read(struct bladerf *dev, uint32_t
    *outputs);
  int bladerf_expansion_gpio_dir_write(struct bladerf *dev, uint32_t
    outputs);
  int bladerf_expansion_gpio_dir_masked_write(struct bladerf *dev,
    uint32_t mask, uint32_t outputs);
  typedef enum
  {
    BLADERF_XB200_50M = 0,
    BLADERF_XB200_144M,
    BLADERF_XB200_222M,
    BLADERF_XB200_CUSTOM,
    BLADERF_XB200_AUTO_1DB,
    BLADERF_XB200_AUTO_3DB
  } bladerf_xb200_filter;
  typedef enum
  {
    BLADERF_XB200_BYPASS = 0,
    BLADERF_XB200_MIX
  } bladerf_xb200_path;
  typedef enum
  {
    BLADERF_XB300_TRX_INVAL = -1,
    BLADERF_XB300_TRX_TX = 0,
    BLADERF_XB300_TRX_RX,
    BLADERF_XB300_TRX_UNSET
  } bladerf_xb300_trx;
  typedef enum
  {
    BLADERF_XB300_AMP_INVAL = -1,
    BLADERF_XB300_AMP_PA = 0,
    BLADERF_XB300_AMP_LNA,
    BLADERF_XB300_AMP_PA_AUX
  } bladerf_xb300_amplifier;
  int bladerf_xb200_set_filterbank(struct bladerf *dev, bladerf_channel
    ch, bladerf_xb200_filter filter);
  int bladerf_xb200_get_filterbank(struct bladerf *dev, bladerf_channel
    ch, bladerf_xb200_filter *filter);
  int bladerf_xb200_set_path(struct bladerf *dev, bladerf_channel ch,
    bladerf_xb200_path path);
  int bladerf_xb200_get_path(struct bladerf *dev, bladerf_channel ch,
    bladerf_xb200_path *path);
  int bladerf_xb300_set_trx(struct bladerf *dev, bladerf_xb300_trx trx);
  int bladerf_xb300_get_trx(struct bladerf *dev, bladerf_xb300_trx
    *trx);
  int bladerf_xb300_set_amplifier_enable(struct bladerf *dev,
    bladerf_xb300_amplifier amp, bool enable);
  int bladerf_xb300_get_amplifier_enable(struct bladerf *dev,
    bladerf_xb300_amplifier amp, bool *enable);
  int bladerf_xb300_get_output_power(struct bladerf *dev, float *val);
  typedef enum
  {
    BLADERF_DC_CAL_INVALID = -1,
    BLADERF_DC_CAL_LPF_TUNING,
    BLADERF_DC_CAL_TX_LPF,
    BLADERF_DC_CAL_RX_LPF,
    BLADERF_DC_CAL_RXVGA2
  } bladerf_cal_module;
  int bladerf_calibrate_dc(struct bladerf *dev, bladerf_cal_module
    module);
  int bladerf_dac_write(struct bladerf *dev, uint16_t val);
  int bladerf_dac_read(struct bladerf *dev, uint16_t *val);
  int bladerf_si5338_read(struct bladerf *dev, uint8_t address, uint8_t
    *val);
  int bladerf_si5338_write(struct bladerf *dev, uint8_t address, uint8_t
    val);
  int bladerf_lms_read(struct bladerf *dev, uint8_t address, uint8_t
    *val);
  int bladerf_lms_write(struct bladerf *dev, uint8_t address, uint8_t
    val);
  struct bladerf_lms_dc_cals
  {
    int16_t lpf_tuning;
    int16_t tx_lpf_i;
    int16_t tx_lpf_q;
    int16_t rx_lpf_i;
    int16_t rx_lpf_q;
    int16_t dc_ref;
    int16_t rxvga2a_i;
    int16_t rxvga2a_q;
    int16_t rxvga2b_i;
    int16_t rxvga2b_q;
  };
  int bladerf_lms_set_dc_cals(struct bladerf *dev, const struct
    bladerf_lms_dc_cals *dc_cals);
  int bladerf_lms_get_dc_cals(struct bladerf *dev, struct
    bladerf_lms_dc_cals *dc_cals);
  int bladerf_xb_spi_write(struct bladerf *dev, uint32_t val);
  int bladerf_get_bias_tee(struct bladerf *dev, bladerf_channel ch, bool
    *enable);
  int bladerf_set_bias_tee(struct bladerf *dev, bladerf_channel ch, bool
    enable);
  int bladerf_get_rfic_register(struct bladerf *dev, uint16_t address,
    uint8_t *val);
  int bladerf_set_rfic_register(struct bladerf *dev, uint16_t address,
    uint8_t val);
  int bladerf_get_rfic_temperature(struct bladerf *dev, float *val);
  int bladerf_get_rfic_rssi(struct bladerf *dev, bladerf_channel ch,
    int32_t *pre_rssi, int32_t *sym_rssi);
  int bladerf_get_rfic_ctrl_out(struct bladerf *dev, uint8_t *ctrl_out);
  typedef enum
  {
    BLADERF_RFIC_RXFIR_BYPASS = 0,
    BLADERF_RFIC_RXFIR_CUSTOM,
    BLADERF_RFIC_RXFIR_DEC1,
    BLADERF_RFIC_RXFIR_DEC2,
    BLADERF_RFIC_RXFIR_DEC4
  } bladerf_rfic_rxfir;
  typedef enum
  {
    BLADERF_RFIC_TXFIR_BYPASS = 0,
    BLADERF_RFIC_TXFIR_CUSTOM,
    BLADERF_RFIC_TXFIR_INT1,
    BLADERF_RFIC_TXFIR_INT2,
    BLADERF_RFIC_TXFIR_INT4
  } bladerf_rfic_txfir;
  int bladerf_get_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir
    *rxfir);
  int bladerf_set_rfic_rx_fir(struct bladerf *dev, bladerf_rfic_rxfir
    rxfir);
  int bladerf_get_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir
    *txfir);
  int bladerf_set_rfic_tx_fir(struct bladerf *dev, bladerf_rfic_txfir
    txfir);
  int bladerf_get_pll_lock_state(struct bladerf *dev, bool *locked);
  int bladerf_get_pll_enable(struct bladerf *dev, bool *enabled);
  int bladerf_set_pll_enable(struct bladerf *dev, bool enable);
  int bladerf_get_pll_refclk_range(struct bladerf *dev, const struct
    bladerf_range **range);
  int bladerf_get_pll_refclk(struct bladerf *dev, uint64_t *frequency);
  int bladerf_set_pll_refclk(struct bladerf *dev, uint64_t frequency);
  int bladerf_get_pll_register(struct bladerf *dev, uint8_t address,
    uint32_t *val);
  int bladerf_set_pll_register(struct bladerf *dev, uint8_t address,
    uint32_t val);
  typedef enum
  {
    BLADERF_UNKNOWN,
    BLADERF_PS_DC,
    BLADERF_PS_USB_VBUS
  } bladerf_power_sources;
  int bladerf_get_power_source(struct bladerf *dev,
    bladerf_power_sources *val);
  typedef enum
  {
    CLOCK_SELECT_ONBOARD,
    CLOCK_SELECT_EXTERNAL
  } bladerf_clock_select;
  int bladerf_get_clock_select(struct bladerf *dev, bladerf_clock_select
    *sel);
  int bladerf_set_clock_select(struct bladerf *dev, bladerf_clock_select
    sel);
  int bladerf_get_clock_output(struct bladerf *dev, bool *state);
  int bladerf_set_clock_output(struct bladerf *dev, bool enable);
  typedef enum
  {
    BLADERF_PMIC_CONFIGURATION,
    BLADERF_PMIC_VOLTAGE_SHUNT,
    BLADERF_PMIC_VOLTAGE_BUS,
    BLADERF_PMIC_POWER,
    BLADERF_PMIC_CURRENT,
    BLADERF_PMIC_CALIBRATION
  } bladerf_pmic_register;
  int bladerf_get_pmic_register(struct bladerf *dev,
    bladerf_pmic_register reg, void *val);
  typedef struct
  {
    uint32_t tx1_rfic_port;
    uint32_t tx1_spdt_port;
    uint32_t tx2_rfic_port;
    uint32_t tx2_spdt_port;
    uint32_t rx1_rfic_port;
    uint32_t rx1_spdt_port;
    uint32_t rx2_rfic_port;
    uint32_t rx2_spdt_port;
  } bladerf_rf_switch_config;
  int bladerf_get_rf_switch_config(struct bladerf *dev,
    bladerf_rf_switch_config *config);
"""
