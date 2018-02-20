library ieee ;
    use ieee.std_logic_1164.all ;

entity nios_system is
  port (
    clk_clk                         : in  std_logic                     := '0';             --                      clk.clk
    command_serial_in               : in  std_logic                     := '0';             --                  command.serial_in
    command_serial_out              : out std_logic;                                        --                         .serial_out
    correction_rx_phase_gain_export : out std_logic_vector(31 downto 0);                    -- correction_rx_phase_gain.export
    correction_tx_phase_gain_export : out std_logic_vector(31 downto 0);                    -- correction_tx_phase_gain.export
    dac_MISO                        : in  std_logic                     := '0';             --                      dac.MISO
    dac_MOSI                        : out std_logic;                                        --                         .MOSI
    dac_SCLK                        : out std_logic;                                        --                         .SCLK
    dac_SS_n                        : out std_logic_vector(1 downto 0);                     --                         .SS_n
    gpio_in_port                    : in  std_logic_vector(31 downto 0);
    gpio_out_port                   : out std_logic_vector(31 downto 0);
    oc_i2c_scl_pad_o                : out std_logic;                                        --                   oc_i2c.scl_pad_o
    oc_i2c_scl_padoen_o             : out std_logic;                                        --                         .scl_padoen_o
    oc_i2c_sda_pad_i                : in  std_logic                     := '0';             --                         .sda_pad_i
    oc_i2c_sda_pad_o                : out std_logic;                                        --                         .sda_pad_o
    oc_i2c_sda_padoen_o             : out std_logic;                                        --                         .sda_padoen_o
    oc_i2c_arst_i                   : in  std_logic                     := '0';             --                         .arst_i
    oc_i2c_scl_pad_i                : in  std_logic                     := '0';             --                         .scl_pad_i
    reset_reset_n                   : in  std_logic                     := '0';             --                    reset.reset_n
    rx_tamer_ts_sync_in             : in  std_logic                     := '0';             --                 rx_tamer.ts_sync_in
    rx_tamer_ts_sync_out            : out std_logic;                                        --                         .ts_sync_out
    rx_tamer_ts_pps                 : in  std_logic                     := '0';             --                         .ts_pps
    rx_tamer_ts_clock               : in  std_logic                     := '0';             --                         .ts_clock
    rx_tamer_ts_reset               : in  std_logic                     := '0';             --                         .ts_reset
    rx_tamer_ts_time                : out std_logic_vector(63 downto 0);                    --                         .ts_time
    spi_MISO                        : in  std_logic                     := '0';             --                      spi.MISO
    spi_MOSI                        : out std_logic;                                        --                         .MOSI
    spi_SCLK                        : out std_logic;                                        --                         .SCLK
    spi_SS_n                        : out std_logic;                                        --                         .SS_n
    tx_tamer_ts_sync_in             : in  std_logic                     := '0';             --                 tx_tamer.ts_sync_in
    tx_tamer_ts_sync_out            : out std_logic;                                        --                         .ts_sync_out
    tx_tamer_ts_pps                 : in  std_logic                     := '0';             --                         .ts_pps
    tx_tamer_ts_clock               : in  std_logic                     := '0';             --                         .ts_clock
    tx_tamer_ts_reset               : in  std_logic                     := '0';             --                         .ts_reset
    tx_tamer_ts_time                : out std_logic_vector(63 downto 0);                    --                         .ts_time
    xb_gpio_in_port                 : in  std_logic_vector(31 downto 0) := (others => '0'); --                  xb_gpio.in_port
    xb_gpio_out_port                : out std_logic_vector(31 downto 0);                    --                         .out_port
    xb_gpio_dir_export              : out std_logic_vector(31 downto 0);                    --              xb_gpio_dir.export
    vctcxo_tamer_tune_ref           : in  std_logic;
    vctcxo_tamer_vctcxo_clock       : in  std_logic;
    tx_trigger_ctl_in_port          : in  std_logic_vector(7 downto 0);
    tx_trigger_ctl_out_port         : out std_logic_vector(7 downto 0);
    rx_trigger_ctl_in_port          : in  std_logic_vector(7 downto 0);
    rx_trigger_ctl_out_port         : out std_logic_vector(7 downto 0)
  );
end entity ;

architecture sim of nios_system is

begin

    command_serial_out <= '1' ;

    correction_rx_phase_gain_export <= x"00001000" ;
    correction_tx_phase_gain_export <= x"00001000" ;

    dac_MOSI <= '0' ;
    dac_SCLK <= '0' ;
    dac_SS_n <= (others =>'1') ;

    gpio_out_port <= x"0001_0157" after 1 us ;

    oc_i2c_scl_pad_o <= '0' ;
    oc_i2c_scl_padoen_o <= '1' ;
    oc_i2c_sda_pad_o <= '0' ;
    oc_i2c_sda_padoen_o <= '1' ;

    rx_tamer_ts_sync_out <= '0' ;
    rx_tamer_ts_time <= (others =>'0') ;

    spi_MOSI <= '0' ;
    spi_SCLK <= '0' ;
    spi_SS_n <= '1' ;

    tx_tamer_ts_sync_out <= '0' ;
    tx_tamer_ts_time <= (others =>'0') ;

    xb_gpio_out_port <= (others =>'0') ;
    xb_gpio_dir_export <= (others =>'0') ;

end architecture ;

