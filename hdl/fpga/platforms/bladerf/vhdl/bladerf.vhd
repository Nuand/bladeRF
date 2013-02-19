library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

library pll ;
library serial_pll ;

entity bladerf is
  port (
    -- Main 38.4MHz system clock
    c4_clock            :   in      std_logic ;

    -- VCTCXO DAC
    dac_sclk            :   out     std_logic ;
    dac_sdi             :   out     std_logic ;
    dac_sdo             :   in      std_logic ;
    dac_csx             :   out     std_logic ;

    -- LEDs
    led                 :   buffer  std_logic_vector(2 downto 1) := (others =>'0') ;

    -- LMS RX Interface
    lms_rx_clock        :   buffer  std_logic ;
    lms_rx_clock_out    :   in      std_logic ;
    lms_rx_data         :   in      signed(11 downto 0) ;
    lms_rx_enable       :   out     std_logic ;
    lms_rx_iq_select    :   in      std_logic := '0' ;
    lms_rx_v            :   out     std_logic_vector(2 downto 1) ;

    -- LMS TX Interface
    lms_tx_clock        :   out     std_logic ;
    lms_tx_data         :   out     signed(11 downto 0) ;
    lms_tx_enable       :   out     std_logic ;
    lms_tx_iq_select    :   buffer  std_logic := '0' ;
    lms_tx_v            :   out     std_logic_vector(2 downto 1) ;

    -- LMS SPI Interface
    lms_sclk            :   buffer  std_logic ;
    lms_sen             :   out     std_logic ;
    lms_sdio            :   out     std_logic ;
    lms_sdo             :   in      std_logic ;

    -- LMS Control Interface
    lms_pll_out         :   in      std_logic ;
    lms_reset           :   buffer  std_logic ;

    -- FX3 Interface
    fx3_pclk            :   in      std_logic ;
    fx3_gpif            :   inout   std_logic_vector(31 downto 0) ;
    fx3_ctl             :   inout   std_logic_vector(12 downto 0) ;
    fx3_uart_rxd        :   out     std_logic ;
    fx3_uart_txd        :   in     std_logic ;
    fx3_uart_csx        :   in      std_logic ;

    -- 1pps reference
    refexp_1pps         :   in      std_logic ;

    -- Expansion Interface
    exp_clock           :   out     std_logic ;
    exp_present         :   in      std_logic ;
    exp_spi_clock       :   out     std_logic ;
    exp_spi_miso        :   in      std_logic ;
    exp_spi_mosi        :   out     std_logic ;
    exp_gpio            :   inout   std_logic_vector(16 downto 1)
  ) ;
end entity ; -- bladerf

architecture arch of bladerf is

    component nios_system is
      port (
        clk_clk       : in  std_logic := 'X'; -- clk
        reset_reset_n : in  std_logic := 'X'; -- reset_n
        dac_MISO      : in  std_logic := 'X'; -- MISO
        dac_MOSI      : out std_logic;        -- MOSI
        dac_SCLK      : out std_logic;        -- SCLK
        dac_SS_n      : out std_logic;        -- SS_n
        spi_MISO      : in  std_logic := 'X'; -- MISO
        spi_MOSI      : out std_logic;        -- MOSI
        spi_SCLK      : out std_logic;        -- SCLK
        spi_SS_n      : out std_logic;        -- SS_n
        uart_rxd      : in  std_logic;
        uart_txd      : out std_logic
      );
    end component nios_system;

    signal ramp_out : signed(11 downto 0) ;

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

    attribute noprune of nios_uart_txd : signal is true ;
    attribute noprune of nios_uart_rxd : signal is true ;

    signal demod_in_i   : signed(15 downto 0) ;
    signal demod_in_q   : signed(15 downto 0) ;
    signal demod_ssd    : signed(15 downto 0) ;
    signal demod_valid  : std_logic ;

    signal qualifier : unsigned(5 downto 0) := (others =>'0') ;
    attribute noprune of qualifier : signal is true ;

begin

    qualifier <= qualifier + 1 when rising_edge(\38.4MHz\) ;

    rx_i <= lms_rx_data when rising_edge(lms_rx_clock_out) and lms_rx_iq_select = '1' ;
    rx_q <= lms_rx_data when rising_edge(lms_rx_clock_out) and lms_rx_iq_select = '0' ;

    U_pll : entity pll.pll
      port map (
        inclk0   =>  c4_clock,
        c0      =>  \76.8MHz\,
        c1      =>  \38.4MHz\,
        c2      =>  \76.8MHz@90\,
        locked  => open
      ) ;

    U_serial_pll : entity serial_pll.serial_pll
      port map (
        inclk0  => c4_clock,
        c0      => rs232_clock,
        locked  => rs232_locked
      ) ;

    -- U_uart_bridge : entity work.uart_bridge
    --   port map (
    --     uart_clock
    --     uart_reset
    --     uart_enable
    --     uart_rxd
    --     uart_txd
    --   ) ;

    -- U_uart : entity work.uart
    --   port map (
    --     clock       => rs232_clock,
    --     reset       => not(rs232_locked),
    --     enable      => not(fx3_uart_csx),

    --     rs232_rxd   => fx3_uart_rxd,
    --     rs232_txd   => fx3_uart_txd,

    --     txd_we      => sfifo_we,
    --     txd_full    => sfifo_full,
    --     txd_data    => sfifo_din,

    --     rxd_re      => sfifo_re,
    --     rxd_empty   => sfifo_empty,
    --     rxd_data    => sfifo_dout
    --   ) ;

    -- U_sfifo : entity work.sync_fifo
    --   generic map (
    --     DEPTH       => 32,
    --     WIDTH       => sfifo_din'length,
    --     READ_AHEAD  => true
    --   ) port map (
    --     areset      => not(rs232_locked),
    --     clock       => rs232_clock,

    --     full        => sfifo_full,
    --     empty       => sfifo_empty,
    --     used_words  => open,

    --     data_in     => sfifo_din,
    --     write_en    => sfifo_we,

    --     data_out    => sfifo_dout,
    --     read_en     => sfifo_re
    --   ) ;

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
        uart_rxd        => nios_uart_rxd,
        uart_txd        => nios_uart_txd
      ) ;

--    U_spi_reader : entity work.spi_reader
--      port map (
--        clock       => c4_clock,
--        sclk        => lms_sclk,
--        miso        => lms_sdo,
--        mosi        => lms_sdio,
--        enx         => lms_sen,
--        reset_out   => lms_reset
--      ) ;

    U_fsk_modulator : entity work.fsk_modulator
      port map (
        clock           => \38.4MHz\,
        reset           => '0',

        symbol          => nios_uart_txd,
        symbol_valid    => '1',

        out_real        => fsk_real,
        out_imag        => fsk_imag,
        out_valid       => fsk_valid
      ) ;

    demod_in_i <= resize(rx_i,demod_in_i'length) when rising_edge(lms_rx_clock_out) and lms_rx_iq_select = '1' ;
    demod_in_q <= resize(rx_q,demod_in_q'length) when rising_edge(lms_rx_clock_out) and lms_rx_iq_Select = '1' ;

    U_fsk_demodulator : entity work.fsk_demodulator
      port map (
        clock           => lms_rx_clock_out,
        reset           => '0',

        in_real         => resize(rx_i,16),
        in_imag         => resize(rx_q,16),
        in_valid        => lms_rx_iq_select,

        out_ssd         => demod_ssd,
        out_valid       => demod_valid
      ) ;

    nios_uart_rxd <= demod_ssd(demod_ssd'high) when demod_valid = '1' ;

    toggle_led1 : process(lms_rx_clock_out)
        variable count : natural range 0 to 38_400_00 := 38_400_00 ;
    begin
        if( rising_edge(lms_rx_clock_out) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 38_400_00 ;
                led(1) <= not led(1) ;
            end if ;
        end if ;
    end process ;

    toggle_led2 : process(c4_clock)
        variable count : natural range 0 to 3_840_000 := 3_840_000 ;
    begin
        if( rising_edge(c4_clock) ) then
            count := count - 1;
            if( count = 0 ) then
                count := 3_840_000 ;
                led(2) <= not led(2) ;
            end if ;
        end if ;
    end process ;


--    -- Digital loopback FX3
--    U_fx3 : entity work.fx3(digital_loopback)
--      port map (
--        pclk    =>  fx3_pclk,
--        gpif    =>  fx3_gpif,
--        ctl     =>  fx3_ctl,
--        rxd     =>  fx3_uart_rxd,
--        txd     =>  fx3_uart_txd,
--        csx     =>  fx3_uart_csx
--      ) ;

    -- Outputs
--    dac_sclk            <= '0' ;
--    dac_sdo             <= '0' ;
--    dac_csx             <= '0' ;

    lms_reset <= '1' ;

    lms_rx_clock        <= \76.8MHz\ ;
    lms_rx_enable       <= '1' ;

    lms_tx_clock        <= \76.8MHz\ ;
    lms_tx_enable       <= '1' ;
    lms_tx_iq_select    <= not lms_tx_iq_select when rising_edge(\76.8MHz\) ;

    assign_data : process(\76.8MHz\)
    begin
        if( rising_edge(\76.8MHz\) ) then
            if( lms_tx_iq_select = '1' ) then
                lms_tx_data <= resize(shift_right(fsk_real,0),lms_tx_data'length) ;
            else
                lms_tx_data <= resize(shift_right(fsk_imag,0),lms_tx_data'length) ;
            end if ;
        end if ;
    end process ;

    U_ramp : entity work.ramp
      port map (
        clock   => c4_clock,
        reset   => '0',
        ramp_out    => ramp_out
      ) ;

    --lms_sclk            <= '0' ;
    --lms_sen             <= '0' ;
    --lms_sdo             <= '0' ;

    lms_tx_v            <= "10" ;
    lms_rx_v            <= "10" ;

    fx3_gpif            <= (others =>'Z') ;
    fx3_ctl             <= (others =>'Z') ;
    -- fx3_uart_rxd        <= fx3_uart_txd ;

    exp_clock           <= '0' ;
    exp_spi_clock       <= '0' ;
    exp_spi_mosi        <= '0' ;
    exp_gpio            <= (others =>'Z') ;

end architecture ; -- arch
