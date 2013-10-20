library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

architecture hosted_bladerf of bladerf is

    attribute noprune   : boolean ;
    attribute keep      : boolean ;

    component nios_system is
      port (
        clk_clk             : in  std_logic := 'X'; -- clk
        reset_reset_n       : in  std_logic := 'X'; -- reset_n
        dac_MISO            : in  std_logic := 'X'; -- MISO
        dac_MOSI            : out std_logic;        -- MOSI
        dac_SCLK            : out std_logic;        -- SCLK
        dac_SS_n            : out std_logic;        -- SS_n
        spi_MISO            : in  std_logic := 'X'; -- MISO
        spi_MOSI            : out std_logic;        -- MOSI
        spi_SCLK            : out std_logic;        -- SCLK
        spi_SS_n            : out std_logic;        -- SS_n
        uart_rxd            : in  std_logic;
        uart_txd            : out std_logic;
        oc_i2c_scl_pad_o    : out std_logic;
        oc_i2c_scl_padoen_o : out std_logic;
        oc_i2c_sda_pad_i    : in  std_logic;
        oc_i2c_sda_pad_o    : out std_logic;
        oc_i2c_sda_padoen_o : out std_logic;
        oc_i2c_arst_i       : in  std_logic;
        oc_i2c_scl_pad_i    : in  std_logic;
        gpio_export         : out std_logic_vector(31 downto 0)
      );
    end component nios_system;

    alias sys_rst   is fx3_ctl(7) ;
    alias tx_clock  is c4_tx_clock ;
    alias rx_clock  is lms_rx_clock_out ;

    signal \80MHz\          : std_logic ;
    signal \80MHz locked\   : std_logic ;
    signal \80MHz reset\    : std_logic ;

    signal nios_gpio        : std_logic_vector(31 downto 0) := x"0000_00d7" ;

    signal i2c_scl_in       : std_logic ;
    signal i2c_scl_out      : std_logic ;
    signal i2c_scl_oen      : std_logic ;

    signal i2c_sda_in       : std_logic ;
    signal i2c_sda_out      : std_logic ;
    signal i2c_sda_oen      : std_logic ;

    type fifo_t is record
        aclr    :   std_logic ;

        wclock  :   std_logic ;
        wdata   :   std_logic_vector(31 downto 0) ;
        wreq    :   std_logic ;
        wempty  :   std_logic ;
        wfull   :   std_logic ;
        wused   :   std_logic_vector(11 downto 0) ;

        rclock  :   std_logic ;
        rdata   :   std_logic_vector(31 downto 0) ;
        rreq    :   std_logic ;
        rempty  :   std_logic ;
        rfull   :   std_logic ;
        rused   :   std_logic_vector(11 downto 0) ;
    end record ;

    signal rx_sample_fifo   : fifo_t ;
    signal tx_sample_fifo   : fifo_t ;

    signal sys_rst_sync     : std_logic ;

    signal usb_speed        : std_logic ;

    signal tx_reset         : std_logic ;
    signal rx_reset         : std_logic ;

    signal pclk_tx_enable   :   std_logic ;
    signal pclk_rx_enable   :   std_logic ;

    signal tx_enable        : std_logic ;
    signal rx_enable        : std_logic ;

    signal rx_sample_i      : signed(11 downto 0) ;
    signal rx_sample_q      : signed(11 downto 0) ;
    signal rx_sample_valid  : std_logic ;

    signal rx_gen_i         : signed(11 downto 0) ;
    signal rx_gen_q         : signed(11 downto 0) ;
    signal rx_gen_valid     : std_logic ;

    signal tx_sample_i      : signed(15 downto 0) ;
    signal tx_sample_q      : signed(15 downto 0) ;
    signal tx_sample_valid  : std_logic ;

    signal fx3_gpif_in      : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_out     : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_oe      : std_logic ;

    signal fx3_ctl_in       : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_out      : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_oe       : std_logic_vector(12 downto 0) ;

    signal tx_underflow_led     :   std_logic ;
    signal tx_underflow_count   :   unsigned(63 downto 0) ;

    signal rx_overflow_led      :   std_logic ;
    signal rx_overflow_count    :   unsigned(63 downto 0) ;

    signal lms_rx_data_reg      :   signed(11 downto 0) ;
    signal lms_rx_iq_select_reg :   std_logic ;

begin

    -- Create 80MHz from 38.4MHz coming from the c4_clock source
    U_pll : entity work.pll
      port map (
        inclk0              =>  c4_clock,
        c0                  =>  \80MHz\,
        locked              =>  \80MHz locked\
      ) ;

    -- Cross domain synchronizer chains
    U_usb_speed : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  fx3_pclk,
        async               =>  nios_gpio(7),
        sync                =>  usb_speed
      ) ;

    U_sys_reset_sync : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  fx3_pclk,
        async               =>  sys_rst,
        sync                =>  sys_rst_sync
      ) ;

    U_tx_reset : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  c4_tx_clock,
        async               =>  sys_rst_sync,
        sync                =>  tx_reset
      ) ;

    U_rx_clock_reset : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  rx_clock,
        async               =>  sys_rst_sync,
        sync                =>  rx_reset
      ) ;

    U_rx_enable_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  pclk_rx_enable,
        sync        =>  rx_enable
      ) ;

    U_tx_enable_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  tx_reset,
        clock       =>  tx_clock,
        async       =>  pclk_tx_enable,
        sync        =>  tx_enable
      ) ;

--    U_80MHz_reset : entity work.reset_synchronizer
--      generic map (
--        INPUT_LEVEL     => '1',
--        OUTPUT_LEVEL    => '0'
--      ) port map (
--        clock           => \80MHz\,
--        async           => sys_rst,
--        sync            => \80MHz reset\
--      ) ;

    -- TX sample fifo
    tx_sample_fifo.aclr <= tx_reset ;
    tx_sample_fifo.wclock <= fx3_pclk ;
    tx_sample_fifo.rclock <= tx_clock ;
    U_tx_sample_fifo : entity work.tx_fifo
      port map (
        aclr                => tx_sample_fifo.aclr,
        data                => tx_sample_fifo.wdata,
        rdclk               => tx_sample_fifo.rclock,
        rdreq               => tx_sample_fifo.rreq,
        wrclk               => tx_sample_fifo.wclock,
        wrreq               => tx_sample_fifo.wreq,
        q                   => tx_sample_fifo.rdata,
        rdempty             => tx_sample_fifo.rempty,
        rdfull              => tx_sample_fifo.rfull,
        rdusedw             => tx_sample_fifo.rused,
        wrempty             => tx_sample_fifo.wempty,
        wrfull              => tx_sample_fifo.wfull,
        wrusedw             => tx_sample_fifo.wused
      );

    -- RX sample fifo
    rx_sample_fifo.wclock <= rx_clock ;
    rx_sample_fifo.rclock <= fx3_pclk ;
    U_rx_sample_fifo : entity work.rx_fifo
      port map (
        aclr                => rx_sample_fifo.aclr,
        data                => rx_sample_fifo.wdata,
        rdclk               => rx_sample_fifo.rclock,
        rdreq               => rx_sample_fifo.rreq,
        wrclk               => rx_sample_fifo.wclock,
        wrreq               => rx_sample_fifo.wreq,
        q                   => rx_sample_fifo.rdata,
        rdempty             => rx_sample_fifo.rempty,
        rdfull              => rx_sample_fifo.rfull,
        rdusedw             => rx_sample_fifo.rused,
        wrempty             => rx_sample_fifo.wempty,
        wrfull              => rx_sample_fifo.wfull,
        wrusedw             => rx_sample_fifo.wused
      );

    -- FX3 GPIF
    U_fx3_gpif : entity work.fx3_gpif
      port map (
        pclk                =>  fx3_pclk,
        reset               =>  sys_rst_sync,

        usb_speed           =>  usb_speed,

        rx_enable           =>  pclk_rx_enable,
        tx_enable           =>  pclk_tx_enable,

        gpif_in             =>  fx3_gpif_in,
        gpif_out            =>  fx3_gpif_out,
        gpif_oe             =>  fx3_gpif_oe,
        ctl_in              =>  fx3_ctl_in,
        ctl_out             =>  fx3_ctl_out,
        ctl_oe              =>  fx3_ctl_oe,

        tx_fifo_write       =>  tx_sample_fifo.wreq,
        tx_fifo_full        =>  tx_sample_fifo.wfull,
        tx_fifo_empty       =>  tx_sample_fifo.wempty,
        tx_fifo_usedw       =>  tx_sample_fifo.wused,
        tx_fifo_data        =>  tx_sample_fifo.wdata,

        rx_fifo_read        =>  rx_sample_fifo.rreq,
        rx_fifo_full        =>  rx_sample_fifo.rfull,
        rx_fifo_empty       =>  rx_sample_fifo.rempty,
        rx_fifo_usedw       =>  rx_sample_fifo.rused,
        rx_fifo_data        =>  rx_sample_fifo.rdata
      ) ;

    -- Sample bridges
    U_fifo_writer : entity work.fifo_writer
      port map (
        clock               =>  rx_clock,
        reset               =>  rx_reset,
        enable              =>  rx_enable,

        fifo_clear          =>  rx_sample_fifo.aclr,
        fifo_full           =>  rx_sample_fifo.wfull,
        fifo_usedw          =>  rx_sample_fifo.wused,
        fifo_data           =>  rx_sample_fifo.wdata,
        fifo_write          =>  rx_sample_fifo.wreq,

        in_i                =>  resize(rx_gen_i,16),
        in_q                =>  resize(rx_gen_q,16),
        in_valid            =>  rx_gen_valid,

        overflow_led        =>  rx_overflow_led,
        overflow_count      =>  rx_overflow_count,
        overflow_duration   =>  x"abcd"
      ) ;

    U_fifo_reader : entity work.fifo_reader
      port map (
        clock               =>  tx_clock,
        reset               =>  tx_reset,
        enable              =>  tx_enable,

        fifo_empty          =>  tx_sample_fifo.rempty,
        fifo_usedw          =>  tx_sample_fifo.rused,
        fifo_data           =>  tx_sample_fifo.rdata,
        fifo_read           =>  tx_sample_fifo.rreq,

        out_i               =>  tx_sample_i,
        out_q               =>  tx_sample_q,
        out_valid           =>  tx_sample_valid,

        underflow_led       =>  tx_underflow_led,
        underflow_count     =>  tx_underflow_count,
        underflow_duration  =>  x"abcd"
      ) ;

    -- LMS6002D IQ interface
    U_lms6002d : entity work.lms6002d
      port map (
        rx_clock            =>  rx_clock,
        rx_reset            =>  rx_reset,
        rx_enable           =>  rx_enable,

        rx_lms_data         =>  lms_rx_data_reg,
        rx_lms_iq_sel       =>  lms_rx_iq_select_reg,
        rx_lms_enable       =>  open,

        rx_sample_i         =>  rx_sample_i,
        rx_sample_q         =>  rx_sample_q,
        rx_sample_valid     =>  rx_sample_valid,

        tx_clock            =>  tx_clock,
        tx_reset            =>  tx_reset,
        tx_enable           =>  tx_enable,

        tx_sample_i         =>  tx_sample_i(11 downto 0),
        tx_sample_q         =>  tx_sample_q(11 downto 0),
        tx_sample_valid     =>  tx_sample_valid,

        tx_lms_data         =>  lms_tx_data,
        tx_lms_iq_sel       =>  lms_tx_iq_select,
        tx_lms_enable       =>  open
      ) ;

    U_rx_siggen : entity work.signal_generator
      port map (
        clock           =>  rx_clock,
        reset           =>  rx_reset,
        enable          =>  rx_enable,

        mode            =>  '0',

        sample_i        =>  rx_gen_i,
        sample_q        =>  rx_gen_q,
        sample_valid    =>  rx_gen_valid
      ) ;

    -- Register the inputs immediately
    lms_rx_data_reg         <= lms_rx_data when rising_edge(rx_clock) ;
    lms_rx_iq_select_reg    <= lms_rx_iq_select when rising_edge(rx_clock) ;

    -- FX3 GPIF bidirectional signals
    register_gpif : process(sys_rst_sync, fx3_pclk)
    begin
        if( sys_rst_sync = '1' ) then
            fx3_gpif <= (others =>'Z') ;
        elsif( rising_edge(fx3_pclk) ) then
            fx3_gpif_in <= fx3_gpif ;
            if( fx3_gpif_oe = '1' ) then
                fx3_gpif <= fx3_gpif_out ;
            else
                fx3_gpif <= (others =>'Z') ;
            end if ;
        end if ;
    end process ;

    generate_ctl : for i in fx3_ctl'range generate
        fx3_ctl(i) <= fx3_ctl_out(i) when fx3_ctl_oe(i) = '1' else 'Z';
    end generate ;

    fx3_ctl_in <= fx3_ctl ;

    -- NIOS control system for si5338, vctcxo trim and lms control
    U_nios_system : nios_system
      port map (
        clk_clk             => \80MHz\,
        reset_reset_n       => '1',
        dac_MISO            => dac_sdo,
        dac_MOSI            => dac_sdi,
        dac_SCLK            => dac_sclk,
        dac_SS_n            => dac_csx,
        spi_MISO            => lms_sdo,
        spi_MOSI            => lms_sdio,
        spi_SCLK            => lms_sclk,
        spi_SS_n            => lms_sen,
        uart_rxd            => fx3_uart_txd,
        uart_txd            => fx3_uart_rxd,
        gpio_export         => nios_gpio,
        oc_i2c_scl_pad_o    => i2c_scl_out,
        oc_i2c_scl_padoen_o => i2c_scl_oen,
        oc_i2c_sda_pad_i    => i2c_sda_in,
        oc_i2c_sda_pad_o    => i2c_sda_out,
        oc_i2c_sda_padoen_o => i2c_sda_oen,
        oc_i2c_arst_i       => '0',
        oc_i2c_scl_pad_i    => i2c_scl_in
      ) ;

    -- IO for NIOS
    si_scl <= i2c_scl_out when i2c_scl_oen = '0' else 'Z' ;
    si_sda <= i2c_sda_out when i2c_sda_oen = '0' else 'Z' ;

    i2c_scl_in <= si_scl ;
    i2c_sda_in <= si_sda ;

    toggle_led1 : process(fx3_pclk)
        variable count : natural range 0 to 100_000_000 := 100_000_000 ;
    begin
        if( rising_edge(fx3_pclk) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 100_000_00 ;
                led(1) <= not led(1) ;
            end if ;
        end if ;
    end process ;

    led(2) <= tx_underflow_led ;
    led(3) <= rx_overflow_led ;

--    toggle_led2 : process(rx_clock)
--        variable count : natural range 0 to 38_400_00 := 38_400_00 ;
--    begin
--        if( rising_edge(rx_clock) ) then
--            count := count - 1 ;
--            if( count = 0 ) then
--                count := 38_400_00 ;
--                led(2) <= not led(2) ;
--            end if ;
--        end if ;
--    end process ;
--
--    toggle_led3 : process(rx_clock)
--        variable count : natural range 0 to 19_200_000 := 19_200_000 ;
--    begin
--        if( rising_edge(rx_clock) ) then
--            count := count - 1 ;
--            if( count = 0 ) then
--                count := 19_200_000 ;
--                led(3) <= not led(3) ;
--            end if ;
--        end if ;
--    end process ;

    lms_reset               <= nios_gpio(0) ;

    lms_rx_enable           <= nios_gpio(1) ;
    lms_tx_enable           <= nios_gpio(2) ;

    lms_tx_v                <= nios_gpio(4 downto 3) ;
    lms_rx_v                <= nios_gpio(6 downto 5) ;

    exp_spi_clock           <= '0' ;
    exp_spi_mosi            <= '0' ;
    exp_gpio                <= (others =>'Z') ;

    mini_exp1               <= 'Z';
    mini_exp2               <= 'Z';

end architecture ; -- arch

