#define kHz(x) (x*1000)
#define MHz(x) (x*1000000)
#define GHz(x) (x*1000000000)

// Frequency selection structure
typedef struct {
    uint8_t x ;
    uint16_t nint ;
    uint32_t nfrac ;
    uint8_t freqsel ;
    uint32_t reference ;
} lms_freq_t ;

// Bandwidth selection
typedef enum {
    BW_28MHz,
    BW_20MHz,
    BW_12MHz,
    BW_14MHz,
    BW_10MHz,
    BW_8p75MHz,
    BW_7MHz,
    BW_6MHz,
    BW_5p5MHz,
    BW_5MHz,
    BW_3p84MHz,
    BW_3MHz,
    BW_2p75MHz,
    BW_2p5MHz,
    BW_1p75MHz,
    BW_1p5MHz,
} lms_bw_t ;

// Module selection for those which have both RX and TX constituents
typedef enum
{
    RX,
    TX
} lms_module_t ;

// Loopback options
typedef enum {
    LB_BB_LPF = 0,
    LB_BB_VGA2,
    LB_BB_OP,
    LB_RF_LNA_START,
    LB_RF_LNA1,
    LB_RF_LNA2,
    LB_RF_LNA3,
    LB_NONE
} lms_loopback_mode_t ;

// LNA options
typedef enum {
    LNA_NONE,
    LNA_1,
    LNA_2,
    LNA_3
} lms_lna_t ;


// LNA gain options
typedef enum {
    LNA_UNKNOWN,
    LNA_BYPASS,
    LNA_MID,
    LNA_MAX
} lms_lna_gain_t ;

typedef enum {
    TXLB_BB,
    TXLB_RF
} lms_txlb_t ;

typedef enum {
    PA_AUX,
    PA_1,
    PA_2,
    PA_ALL
} lms_pa_t ;

// Frequency Range table
typedef struct {
    uint32_t low ;
    uint32_t high ;
    uint8_t value ;
} freq_range_t ;

typedef struct {
    uint32_t tx_freq_hz;
    uint32_t rx_freq_hz;
    lms_loopback_mode_t loopback_mode;
    lms_lna_t lna;
    lms_pa_t  pa;
    lms_bw_t bw;
} xcvr_config_t;

// When enabling an LPF, we must select both the module and the filter bandwidth
void lms_lpf_enable( struct bladerf *dev, lms_module_t mod, lms_bw_t bw );

void lms_lpf_bypass( struct bladerf *dev, lms_module_t mod );

// Disable the LPF for a specific module
void lms_lpf_disable( struct bladerf *dev, lms_module_t mod );

// Get the bandwidth for the selected module
lms_bw_t lms_get_bandwidth( struct bladerf *dev, lms_module_t mod );

// Enable dithering on the module PLL
void lms_dither_enable( struct bladerf *dev, lms_module_t mod, uint8_t nbits );

// Disable dithering on the module PLL
void lms_dither_disable( struct bladerf *dev, lms_module_t mod );
// Soft reset of the LMS
void lms_soft_reset( struct bladerf *dev );

// Set the gain on the LNA
void lms_lna_set_gain( struct bladerf *dev, lms_lna_gain_t gain );

// Select which LNA to enable
void lms_lna_select( struct bladerf *dev, lms_lna_t lna );

// Disable RXVGA1
void lms_rxvga1_disable(struct bladerf *dev );

// Enable RXVGA1
void lms_rxvga1_enable(struct bladerf *dev );

// Disable RXVGA2
void lms_rxvga2_disable(struct bladerf *dev );

// Set the gain on RXVGA2
void lms_rxvga2_set_gain( struct bladerf *dev, uint8_t gain );
// Enable RXVGA2
void lms_rxvga2_enable( struct bladerf *dev, uint8_t gain );

void lms_set_txvga2_gain( struct bladerf *dev, uint8_t gain ) ;

// Enable PA (PA_ALL is NOT valid for enabling)
void lms_pa_enable( struct bladerf *dev, lms_pa_t pa );

// Disable PA
void lms_pa_disable( struct bladerf *dev, lms_pa_t pa );

void lms_peakdetect_enable( struct bladerf *dev );

void lms_peakdetect_disable( struct bladerf *dev );
// Enable TX loopback
void lms_tx_loopback_enable( struct bladerf *dev, lms_txlb_t mode );
// Disable TX loopback
void lms_tx_loopback_disable( struct bladerf *dev, lms_txlb_t mode );
// Loopback enable
void lms_loopback_enable( struct bladerf *dev, lms_loopback_mode_t mode );
// Figure out what loopback mode we're in (if any at all!)
lms_loopback_mode_t lms_get_loopback_mode( struct bladerf *dev );

// Disable loopback mode - must choose which LNA to hook up and what bandwidth you want
void lms_loopback_disable( struct bladerf *dev, lms_lna_t lna, lms_bw_t bw );

// Top level power down of the LMS
void lms_power_down( struct bladerf *dev );

// Enable the PLL of a module
void lms_pll_enable( struct bladerf *dev, lms_module_t mod );
// Disable the PLL of a module
void lms_pll_disable( struct bladerf *dev, lms_module_t mod );
// Enable the RX subsystem
void lms_rx_enable( struct bladerf *dev );
// Disable the RX subsystem
void lms_rx_disable( struct bladerf *dev );

// Enable the TX subsystem
void lms_tx_enable( struct bladerf *dev );

// Disable the TX subsystem
void lms_tx_disable( struct bladerf *dev );
// Print a frequency structure
void lms_print_frequency( lms_freq_t *f );
// Get the frequency structure
void lms_get_frequency( struct bladerf *dev, lms_module_t mod, lms_freq_t *f );
// Set the frequency of a module
void lms_set_frequency( struct bladerf *dev, lms_module_t mod, uint32_t freq );

void lms_dump_registers( struct bladerf *dev );

void lms_calibrate_dc( struct bladerf *dev );

void lms_lpf_init( struct bladerf *dev );

int lms_config_init( struct bladerf *dev, xcvr_config_t *config);
