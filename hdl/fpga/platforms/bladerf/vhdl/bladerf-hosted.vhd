library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

architecture hosted_bladerf of bladerf is

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

    signal ramp_out : signed(11 downto 0) ;

    signal lms_tx_clock :   std_logic ;

    signal \38.4MHz\    :   std_logic ;
    signal \76.8MHz\    :   std_logic ;
    signal \76.8MHz@90\ :   std_logic ;

    signal rs232_clock  :   std_logic ;
    signal rs232_locked :   std_logic ;

    signal sfifo_din    :   std_logic_vector(7 downto 0) ;
    signal sfifo_dout   :   std_logic_vector(7 downto 0) ;
    signal sfifo_full   :   std_logic ;
    signal sfifo_empty  :   std_logic ;
    signal sfifo_re     :   std_logic ;
    signal sfifo_we     :   std_logic ;

    attribute noprune : boolean ;

    signal rx_i         :   signed(11 downto 0) ;
    signal rx_q         :   signed(11 downto 0) ;

    attribute noprune of rx_i : signal is true ;
    attribute noprune of rx_q : signal is true ;

    signal fsk_real     : signed(15 downto 0) ;
    signal fsk_imag     : signed(15 downto 0) ;
    signal fsk_valid    : std_logic ;

    attribute noprune of fsk_real : signal is true ;
    attribute noprune of fsk_imag : signal is true ;

    signal nios_uart_txd : std_logic ;
    signal nios_uart_rxd : std_logic ;

    signal nios_gpio     : std_logic_vector(31 downto 0) ;

    attribute noprune of nios_uart_txd : signal is true ;
    attribute noprune of nios_uart_rxd : signal is true ;

    signal demod_in_i   : signed(15 downto 0) ;
    signal demod_in_q   : signed(15 downto 0) ;
    signal demod_ssd    : signed(15 downto 0) ;
    signal demod_valid  : std_logic ;

    signal qualifier : unsigned(5 downto 0) := (others =>'0') ;
    attribute noprune of qualifier : signal is true ;

    signal i2c_scl_in  : std_logic ;
    signal i2c_scl_out : std_logic ;
    signal i2c_scl_oen : std_logic ;

    signal i2c_sda_in  : std_logic ;
    signal i2c_sda_out : std_logic ;
    signal i2c_sda_oen : std_logic ;

    signal gpif_var            : std_logic_vector(31 downto 0) ;

    signal rf_fifo_rcnt : signed(12 downto 0);

    --- RF rx FIFO signals
    signal rf_rx_fifo_full    : std_logic;
    signal rf_rx_fifo_clr     : std_logic;
    signal rf_rx_fifo_read    : std_logic;
    signal rf_rx_fifo_cnt     : std_logic_vector(9 downto 0);
    signal rf_rx_fifo_q       : std_logic_vector(31 downto 0);
    signal rf_rx_fifo_w       : std_logic;

    signal rf_rx_fifo_enough  : std_logic;

    signal rf_rx_fifo_sample  : signed(31 downto 0) ;
    signal rf_rx_last_sample  : signed(11 downto 0) ;
    signal rf_rx_sample_idx   : signed(2 downto 0) ;
    --- end RF rx FIFO

    --- RF tx FIFO signals
    signal rf_tx_fifo_clr    : std_logic;
    signal rf_tx_fifo_data   : std_logic_vector(31 downto 0);
    signal rf_tx_fifo_read   : std_logic;
    signal rf_tx_fifo_w      : std_logic;
    signal rf_tx_fifo_q      : std_logic_vector(31 downto 0);
    signal rf_tx_fifo_empty  : std_logic;
    signal rf_tx_fifo_cnt    : std_logic_vector(11 downto 0);

    signal rf_tx_fifo_enough  : std_logic;

    signal rf_tx_fifo_data_iq_r    : std_logic_vector(31 downto 0);
    signal rf_tx_fifo_data_iq_rr   : std_logic_vector(31 downto 0);
    signal tx_data           :   std_logic_vector(11 downto 0) ;
    --- end RF tx FIFO

    signal debug_line_speed : std_logic;
    signal debug_line_speed_rx, debug_line_speed_tx : std_logic;

    signal can_perform_rx, should_perform_rx : std_logic;
    signal can_perform_tx, should_perform_tx : std_logic;

    signal rf_tx_next_dma : std_logic;
    signal rf_tx_dma_2  : std_logic;
    signal rf_tx_dma_3  : std_logic;

    signal rf_rx_next_dma : std_logic;
    signal rf_rx_dma_0  : std_logic;
    signal rf_rx_dma_1  : std_logic;

    signal sys_rst : std_logic;

    signal dma_idle : std_logic;
    signal dma_rdy_0 : std_logic;
    signal dma_rdy_1 : std_logic;
    signal dma_rdy_2 : std_logic;
    signal dma_rdy_3 : std_logic;
    type dma_event is (DE_TX, DE_RX);
    signal dma_last_event : dma_event;
    signal dma_rx_en : std_logic;
    signal dma_tx_en : std_logic;

    signal dma_rx_en_r  : std_logic;
    signal dma_rx_en_rr : std_logic;
    signal dma_tx_en_r  : std_logic;
    signal dma_tx_en_rr : std_logic;
    signal rf_tx_en_iq_r  : std_logic;
    signal rf_tx_en_iq_rr : std_logic;

    signal tx_iq_idx : std_logic;

    type m_state is (M_IDLE, M_IDLE_RD, M_IDLE_WR, M_READ, M_WRITE);
    signal current_state : m_state;

    attribute keep: boolean;
    attribute keep of dma_idle: signal is true;
    attribute keep of rf_rx_fifo_cnt: signal is true;
    attribute keep of rf_rx_fifo_enough: signal is true;
    attribute keep of dma_rdy_0: signal is true;
    attribute keep of dma_rdy_1: signal is true;
    attribute keep of rf_rx_next_dma: signal is true;
    attribute keep of sys_rst: signal is true;
    attribute keep of rf_rx_fifo_full: signal is true;
    attribute keep of rf_rx_fifo_clr: signal is true;
    --attribute keep of lms_rx_clock: signal is true;

    attribute keep of can_perform_rx: signal is true;
    attribute keep of can_perform_tx: signal is true;
    attribute keep of should_perform_rx: signal is true;
    attribute keep of should_perform_tx: signal is true;
    attribute keep of rf_tx_fifo_enough: signal is true;
    attribute keep of rf_tx_fifo_cnt: signal is true;
    attribute keep of rf_tx_fifo_w: signal is true;

    attribute noprune of dma_idle: signal is true;
    attribute noprune of rf_rx_fifo_cnt: signal is true;
    attribute noprune of rf_rx_fifo_enough: signal is true;
    attribute noprune of dma_rdy_0: signal is true;
    attribute noprune of dma_rdy_1: signal is true;
    attribute noprune of rf_rx_next_dma: signal is true;
    attribute noprune of sys_rst: signal is true;
    attribute noprune of rf_rx_fifo_full: signal is true;
    attribute noprune of rf_rx_fifo_clr: signal is true;
    --attribute noprune of lms_rx_clock: signal is true;

    attribute noprune of can_perform_rx: signal is true;
    attribute noprune of can_perform_tx: signal is true;
    attribute noprune of should_perform_rx: signal is true;
    attribute noprune of should_perform_tx: signal is true;
    attribute noprune of rf_tx_fifo_enough: signal is true;
    attribute noprune of rf_tx_fifo_cnt: signal is true;
    attribute noprune of rf_tx_fifo_w: signal is true;

begin

    qualifier <= qualifier + 1 when rising_edge(\38.4MHz\) ;

    rx_i <= lms_rx_data when rising_edge(lms_rx_clock_out) and lms_rx_iq_select = '1' ;
    rx_q <= lms_rx_data when rising_edge(lms_rx_clock_out) and lms_rx_iq_select = '0' ;

    U_pll : entity work.pll
      port map (
        inclk0   =>  c4_tx_clock,
        c0      =>  \76.8MHz\,
        c1      =>  \38.4MHz\,
        c2      =>  \76.8MHz@90\,
        locked  => open
      ) ;

    U_serial_pll : entity work.serial_pll
      port map (
        inclk0  => c4_clock,
        c0      => rs232_clock,
        locked  => rs232_locked
      ) ;

    fx3_ctl(0) <= rf_rx_dma_0;
    fx3_ctl(1) <= rf_rx_dma_1;
    fx3_ctl(2) <= rf_tx_dma_2;
    fx3_ctl(3) <= rf_tx_dma_3;

    dma_rx_en  <= fx3_ctl(4);
    dma_tx_en  <= fx3_ctl(5);
    dma_idle   <= fx3_ctl(6);
    sys_rst    <= fx3_ctl(7);

    dma_rdy_0  <= fx3_ctl(8);
    dma_rdy_1  <= fx3_ctl(12); -- 9 is DCLK, it is somewhat lost
    dma_rdy_2  <= fx3_ctl(10);
    dma_rdy_3  <= fx3_ctl(11);

    rf_tx_fifo : entity work.tx_fifo
      port map (
        aclr      => rf_tx_fifo_clr,
        data      => rf_tx_fifo_data,
        rdclk     => \38.4MHz\,
        rdreq     => dma_tx_en_rr,
        wrclk     => fx3_pclk,
        wrreq     => rf_tx_fifo_w,
        q         => rf_tx_fifo_q,
        rdempty   => rf_tx_fifo_empty,
        rdfull    => open,
        rdusedw   => open,
        wrempty   => open,
        wrfull    => open,
        wrusedw   => rf_tx_fifo_cnt
      );
    rf_tx_fifo_enough <= '1' when (unsigned(rf_tx_fifo_cnt) <= ((2**(rf_tx_fifo_cnt'length-1))  - 512)) else '0';
    rf_tx_fifo_clr <= '1' when (sys_rst = '1') else '0';

    rf_tx_fifo_w <= '1' when (current_state = M_WRITE) else '0';

    process(sys_rst, c4_clock)
    begin
        if( sys_rst = '1' ) then
            dma_tx_en_r <= '0';
            dma_tx_en_rr <= '0';
        elsif( rising_edge(c4_clock) ) then
            dma_tx_en_r <= dma_tx_en;
            dma_tx_en_rr <= dma_tx_en_r;
        end if;
    end process;

    process(sys_rst, \76.8MHz\)
    begin
        if( sys_rst = '1' ) then
            rf_tx_en_iq_r <= '0';
            rf_tx_en_iq_rr <= '0';
            rf_tx_fifo_data_iq_r <= (others => '0');
            rf_tx_fifo_data_iq_rr <= (others => '0');
        elsif( rising_edge(\76.8MHz\) ) then
            rf_tx_en_iq_r <= not rf_tx_fifo_empty;
            rf_tx_en_iq_rr <= rf_tx_en_iq_r;
            rf_tx_fifo_data_iq_r <= rf_tx_fifo_q;
            rf_tx_fifo_data_iq_rr <= rf_tx_fifo_data_iq_r;
        end if;
    end process;

    process(sys_rst, \76.8MHz\)
    begin
        if( sys_rst = '1' ) then
            tx_iq_idx <= '0';
        elsif( rising_edge(\76.8MHz\) ) then
            if (rf_tx_en_iq_rr = '1') then
                tx_iq_idx <= not tx_iq_idx;
            elsif (rf_tx_en_iq_rr = '0') then
                tx_iq_idx <= '0';
            end if;
        end if;
    end process;

    tx_data <= rf_tx_fifo_data_iq_rr(27 downto 16) when tx_iq_idx = '0' else rf_tx_fifo_data_iq_rr(11 downto 0);
    lms_tx_data <= signed(tx_data) when rf_tx_en_iq_rr = '1' else (others => '0');

    rf_rx_fifo : entity work.rx_fifo
      port map (
        aclr      => rf_rx_fifo_clr,
        data      => std_logic_vector(rf_rx_fifo_sample),
        rdclk     => fx3_pclk,
        rdreq     => rf_rx_fifo_read,
        wrclk     => lms_rx_clock_out,
        wrreq     => rf_rx_fifo_w,
        q         => rf_rx_fifo_q,
        rdempty   => open,
        rdfull    => open,
        rdusedw   => rf_rx_fifo_cnt,
        wrempty   => open,
        wrfull    => rf_rx_fifo_full,
        wrusedw   => open
      );

    rf_rx_fifo_enough <= '1' when (unsigned(rf_rx_fifo_cnt) >= 512 ) else '0';

    rf_rx_fifo_clr <= '1' when (sys_rst = '1' or (rf_rx_fifo_full = '1' and signed(rf_rx_sample_idx) = 0)) else '0';
    rf_rx_fifo_read <= '1' when (current_state = M_READ) else '0';

    process(all)
    begin
        if( current_state = M_READ or current_state = M_IDLE_RD) then
            fx3_gpif <= rf_rx_fifo_q;
        elsif( current_state = M_WRITE or current_state = M_IDLE_WR) then
            rf_tx_fifo_data <= fx3_gpif;
        else
            fx3_gpif <= (others => 'Z');
        end if;
    end process;
    --todo: readd debug_line_speed handling
    --fx3_gpif <= rf_rx_fifo_q when (debug_line_speed_rx = '0' and (current_state = M_READ or current_state = M_IDLE_RD)) else (others => 'Z');
    --gpif_var <= fx3_gpif;
    --process(all)
    --begin
    --    if (debug_line_speed_rx = '0' and (current_state = M_READ or current_state = M_IDLE_RD)) then
    --        fx3_gpif <= rf_rx_fifo_q;
    --    elsif (current_state = M_WRITE ) then
    --        gpif_var <= fx3_gpif;
    --    else
    --        fx3_gpif <= (others => 'Z');
    --    end if;
    --end process;

    debug_line_speed <= '0';
    debug_line_speed_rx <= debug_line_speed;
    debug_line_speed_tx <= debug_line_speed;

    can_perform_rx <= '1' when (dma_rx_en = '1' and (
                                    debug_line_speed_rx = '1' or
                                    (rf_rx_fifo_enough = '1' and (
                                          (dma_rdy_0 = '0' and rf_rx_next_dma = '0') or
                                          (dma_rdy_1 = '0' and rf_rx_next_dma = '1')
                                          )
                                    )
                                )) else '0';

    can_perform_tx <= '1' when (dma_tx_en = '1' and (
                                    debug_line_speed_tx = '1' or
                                    (rf_tx_fifo_enough = '1' and (
                                          (dma_rdy_2 = '0' and rf_tx_next_dma = '0') or
                                          (dma_rdy_3 = '0' and rf_tx_next_dma = '1')
                                          )
                                    )
                                )) else '0';

    should_perform_rx <= '1' when ( can_perform_rx = '1' and (can_perform_tx = '0' or (can_perform_tx = '1' and dma_last_event = DE_TX ) ) ) else '0';

    should_perform_tx <= '1' when ( can_perform_tx = '1' and (can_perform_rx = '0' or (can_perform_rx = '1' and dma_last_event = DE_RX ) ) ) else '0';

    process(sys_rst, fx3_pclk)
    begin
        if( sys_rst = '1' ) then
            current_state <= M_IDLE;
            rf_tx_next_dma <= '0';
            rf_rx_next_dma <= '0';
            rf_rx_dma_0 <= '0';
            rf_rx_dma_1 <= '0';
            rf_tx_dma_2 <= '0';
            rf_tx_dma_3 <= '0';
            rf_fifo_rcnt <= (others => '0');
            dma_last_event <= DE_TX;
        elsif( rising_edge(fx3_pclk) ) then
            case current_state is
                when M_IDLE =>
                    if( dma_idle = '1' ) then
                        if( should_perform_rx = '1' ) then
                            rf_fifo_rcnt <= to_signed(512, 13);

                            if ( rf_rx_next_dma = '0') then
                                rf_rx_dma_0 <= '1';
                                rf_rx_dma_1 <= '0';
                            else
                                rf_rx_dma_0 <= '0';
                                rf_rx_dma_1 <= '1';
                            end if;

                            rf_rx_next_dma <= not rf_rx_next_dma;

                            current_state <= M_IDLE_RD;
                            -- set this to DE_RX unconditionally so that no hangs occur
                            -- if there is an problem with RX
                            dma_last_event <= DE_RX;
                        elsif( should_perform_tx = '1' ) then
                            rf_fifo_rcnt <= to_signed(512, 13);

                            if( rf_tx_next_dma = '0') then
                                rf_tx_dma_2 <= '1';
                                rf_tx_dma_3 <= '0';
                            else
                                rf_tx_dma_2 <= '0';
                                rf_tx_dma_3 <= '1';
                            end if;

                            rf_tx_next_dma <= not rf_tx_next_dma;

                            current_state <= M_IDLE_WR;

                            dma_last_event <= DE_TX;
                        end if;
                    end if;

                when M_IDLE_WR =>
                    current_state <= M_WRITE;
                when M_WRITE =>
                    rf_tx_dma_2 <= '0';
                    rf_tx_dma_3 <= '0';
                    if( unsigned(rf_fifo_rcnt) /= 0 ) then
                        rf_fifo_rcnt <= rf_fifo_rcnt - 1;
                    else
                        current_state <= M_IDLE;
                    end if;
                when M_IDLE_RD =>
                    current_state <= M_READ;
                when M_READ =>
                    rf_rx_dma_0 <= '0';
                    rf_rx_dma_1 <= '0';

                    if( unsigned(rf_fifo_rcnt) /= 0 ) then
                        rf_fifo_rcnt <= rf_fifo_rcnt - 1;
                    else
                        current_state <= M_IDLE;
                    end if;
            end case;

        end if;
    end process;

    -- |      Byte 1     |      Byte 2            Byte 3            Byte 4     |
    -- | 0 0 0 0 0 0 0 0 | 0 0 0 0 1 1 1 1 | 1 1 1 1 1 1 1 1 | 2 2 2 2 2 2 2 2 |
    -- | 2 2 2 2 3 3 3 3 | 3 3 3 3 3 3 3 3 | 4 4 4 4 4 4 4 4 | 4 4 4 4 5 5 5 5 |
    -- | 5 5 5 5 5 5 5 5 | 6 6 6 6 6 6 6 6 | 6 6 6 6 7 7 7 7 | 7 7 7 7 7 7 7 7 |
    --
    -- Eight 12bit samples have to be collected to align data being fed into the FIFO
    -- Enough data exists at the end of RF samples #2, #5, #7 to write data to the FIFO
    process(sys_rst, lms_rx_clock_out)
    begin
        if( sys_rst = '1' ) then
            dma_rx_en_r <= '0';
            dma_rx_en_rr <= '0';
        elsif( rising_edge(lms_rx_clock_out) ) then
            dma_rx_en_r <= dma_rx_en;
            dma_rx_en_rr <= dma_rx_en_r;
        end if;
    end process;


    process(sys_rst, lms_rx_clock_out)
    begin
        if( sys_rst = '1' ) then
            rf_rx_fifo_sample <= (others => '0');
            rf_rx_last_sample <= (others => '0');
            rf_rx_sample_idx <= (others => '0');
            rf_rx_fifo_w <= '0';
        elsif( rising_edge(lms_rx_clock_out) ) then
            if( dma_rx_en_rr = '1' ) then
                --rf_rx_last_sample <= upcnter;

                --if( unsigned(rf_rx_sample_idx) = 2 or unsigned(rf_rx_sample_idx) = 5 or unsigned(rf_rx_sample_idx) = 7) then
                --    rf_rx_fifo_w <= '1';
                --else
                --    rf_rx_fifo_w <= '0';
                --end if;

                rf_rx_fifo_sample(31 downto 0) <= "1011" & rx_i & "0011" & rx_q;
                rf_rx_fifo_w <= not lms_rx_iq_select;
                --if ( rf_rx_sample_idx(0) = '0')
                --rf_rx_fifo_sample(11 downto 0) <= upcnter;
                --if( unsigned(rf_rx_sample_idx) = 0 ) then
                --    rf_rx_fifo_sample(11 downto 0)  <= upcnter(11 downto 0);
                --elsif( unsigned(rf_rx_sample_idx) = 1 ) then
                --    rf_rx_fifo_sample(23 downto 12) <= upcnter(11 downto 0);
                --elsif( unsigned(rf_rx_sample_idx) = 2 ) then
                --    rf_rx_fifo_sample(31 downto 24) <= upcnter(7 downto 0);
                --elsif( unsigned(rf_rx_sample_idx) = 3 ) then
                --    rf_rx_fifo_sample(15 downto 0)  <= upcnter(11 downto 0) & rf_rx_last_sample(11 downto 8);
                --elsif( unsigned(rf_rx_sample_idx) = 4 ) then
                --    rf_rx_fifo_sample(27 downto 16) <= upcnter(11 downto 0);
                --elsif( unsigned(rf_rx_sample_idx) = 5 ) then
                --    rf_rx_fifo_sample(31 downto 28) <= upcnter(3 downto 0);
                --elsif( unsigned(rf_rx_sample_idx) = 6 ) then
                --    rf_rx_fifo_sample(19 downto 0)  <= upcnter(11 downto 0) & rf_rx_last_sample(11 downto 4);
                --elsif( unsigned(rf_rx_sample_idx) = 7 ) then
                --    rf_rx_fifo_sample(31 downto 20) <= upcnter(11 downto 0);
                --end if;
                rf_rx_sample_idx <= rf_rx_sample_idx + 1;
            end if ;
        end if ;
    end process;


    U_nios_system : nios_system
      port map (
        clk_clk         => c4_clock,
        reset_reset_n   => '1',
        dac_MISO        => dac_sdo,
        dac_MOSI        => dac_sdi,
        dac_SCLK        => dac_sclk,
        dac_SS_n        => dac_csx,
        spi_MISO        => lms_sdo,
        spi_MOSI        => lms_sdio,
        spi_SCLK        => lms_sclk,
        spi_SS_n        => lms_sen,
        uart_rxd        => fx3_uart_txd,
        uart_txd        => fx3_uart_rxd,
        gpio_export     => nios_gpio,
        oc_i2c_scl_pad_o    => i2c_scl_out,
        oc_i2c_scl_padoen_o => i2c_scl_oen,
        oc_i2c_sda_pad_i    => i2c_sda_in,
        oc_i2c_sda_pad_o    => i2c_sda_out,
        oc_i2c_sda_padoen_o => i2c_sda_oen,
        oc_i2c_arst_i       => '0',
        oc_i2c_scl_pad_i    => i2c_scl_in
      ) ;

    si_scl <= i2c_scl_out when i2c_scl_oen = '0' else 'Z' ;
    si_sda <= i2c_sda_out when i2c_sda_oen = '0' else 'Z' ;

    i2c_scl_in <= si_scl ;
    i2c_sda_in <= si_sda ;

    nios_uart_rxd <= demod_ssd(demod_ssd'high) when demod_valid = '1' ;

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

    toggle_led2 : process(lms_rx_clock_out)
        variable count : natural range 0 to 38_400_00 := 38_400_00 ;
    begin
        if( rising_edge(lms_rx_clock_out) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 38_400_00 ;
                led(2) <= not led(2) ;
            end if ;
        end if ;
    end process ;

    toggle_led3 : process(lms_rx_iq_select)
        variable count : natural range 0 to 19_200_000 := 19_200_000 ;
    begin
        if( rising_edge(lms_rx_iq_select) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 19_200_000 ;
                led(3) <= not led(3) ;
            end if ;
        end if ;
    end process ;

    lms_reset <= nios_gpio(0) ;

    lms_rx_enable       <= nios_gpio(1) ;

    lms_tx_enable       <= nios_gpio(2) ;
    lms_tx_iq_select    <= not lms_tx_iq_select when rising_edge(\76.8MHz\) ;

    lms_tx_v            <= nios_gpio(4 downto 3) ;
    lms_rx_v            <= nios_gpio(6 downto 5) ;

    exp_spi_clock       <= '0' ;
    exp_spi_mosi        <= '0' ;
    exp_gpio            <= (others =>'Z') ;

end architecture ; -- arch

