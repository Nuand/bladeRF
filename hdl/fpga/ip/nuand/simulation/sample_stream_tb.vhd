library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;

library nuand ;
    use nuand.util.all ;

library std ;
    use std.env.all ;
    use std.textio.all ;

entity sample_stream_tb is
end entity ;

architecture arch of sample_stream_tb is

    -- Checking constants
    constant CHECK_OVERFLOW     :   boolean     := false ;
    constant CHECK_UNDERFLOW    :   boolean     := false ;

    -- Clock half periods
    constant FX3_HALF_PERIOD    :   time        := 1.0/(100.0e6)/2.0*1 sec ;
    constant TX_HALF_PERIOD     :   time        := 1.0/(40.0e6)/2.0*1 sec ;
    constant RX_HALF_PERIOD     :   time        := 1.0/(40.0e6)/2.0*1 sec ;

    -- Clocks
    signal fx3_clock            :   std_logic   := '1' ;
    signal tx_clock             :   std_logic   := '1' ;
    signal rx_clock             :   std_logic   := '1' ;

    signal reset                :   std_logic   := '1' ;

    -- TX Signalling
    signal tx_enable            :   std_logic ;
    signal tx_native_i          :   signed(11 downto 0) ;
    signal tx_native_q          :   signed(11 downto 0) ;
    signal tx_sample_i          :   signed(15 downto 0) ;
    signal tx_sample_q          :   signed(15 downto 0) ;
    signal tx_sample_valid      :   std_logic ;

    -- RX Signalling
    signal rx_enable            :   std_logic ;
    signal rx_native_i          :   signed(11 downto 0) ;
    signal rx_native_q          :   signed(11 downto 0) ;
    signal rx_sample_i          :   signed(15 downto 0) ;
    signal rx_sample_q          :   signed(15 downto 0) ;
    signal rx_sample_valid      :   std_logic ;

    -- Underflow
    signal underflow_led        :   std_logic ;
    signal underflow_count      :   unsigned(63 downto 0) ;
    signal underflow_duration   :   unsigned(15 downto 0) := to_unsigned(20, 16) ;

    -- Overflow
    signal overflow_led         :   std_logic ;
    signal overflow_count       :   unsigned(63 downto 0) ;
    signal overflow_duration    :   unsigned(15 downto 0) := to_unsigned(20, 16) ;

    -- LMS signals
    signal lms_rx_data          :   signed(11 downto 0) ;
    signal lms_rx_iq_select     :   std_logic ;
    signal lms_rx_enable        :   std_logic ;

    signal lms_tx_data          :   signed(11 downto 0) ;
    signal lms_tx_iq_select     :   std_logic ;
    signal lms_tx_enable        :   std_logic ;

    -- FIFO type
    type fifo_t is record
        aclr    :   std_logic ;
        data    :   std_logic_vector(31 downto 0) ;
        rdclk   :   std_logic ;
        rdreq   :   std_logic ;
        wrclk   :   std_logic ;
        wrreq   :   std_logic ;
        q       :   std_logic_vector(31 downto 0) ;
        rdempty :   std_logic ;
        rdfull  :   std_logic ;
        rdusedw :   std_logic_vector ;
        wrempty :   std_logic ;
        wrfull  :   std_logic ;
        wrusedw :   std_logic_vector ;
    end record ;

    -- TX FIFO
    signal txfifo       :   fifo_t( rdusedw(11 downto 0), wrusedw(11 downto 0) ) ;
    signal rxfifo       :   fifo_t( rdusedw( 9 downto 0), wrusedw( 9 downto 0) ) ;

    alias lms_tx_clock  :   std_logic is tx_clock ;
    alias lms_rx_clock  :   std_logic is rx_clock ;

begin

    -- Clock creation
    fx3_clock   <= not fx3_clock after FX3_HALF_PERIOD ;
    tx_clock    <= not tx_clock after TX_HALF_PERIOD ;
    rx_clock    <= not rx_clock after RX_HALF_PERIOD ;

    -- TX FIFO
    txfifo.aclr <= reset ;
    txfifo.rdclk <= tx_clock ;
    txfifo.wrclk <= fx3_clock ;
    U_tx_fifo : entity work.tx_fifo
      port map (
        aclr                =>  txfifo.aclr,
        data                =>  txfifo.data,
        rdclk               =>  txfifo.rdclk,
        rdreq               =>  txfifo.rdreq,
        wrclk               =>  txfifo.wrclk,
        wrreq               =>  txfifo.wrreq,
        q                   =>  txfifo.q,
        rdempty             =>  txfifo.rdempty,
        rdfull              =>  txfifo.rdfull,
        rdusedw             =>  txfifo.rdusedw,
        wrempty             =>  txfifo.wrempty,
        wrfull              =>  txfifo.wrfull,
        wrusedw             =>  txfifo.wrusedw
      ) ;

    -- RX FIFO
    rxfifo.rdclk <= fx3_clock ;
    rxfifo.wrclk <= rx_clock ;
    U_rx_fifo : entity work.rx_fifo
      port map (
        aclr                =>  rxfifo.aclr,
        data                =>  rxfifo.data,
        rdclk               =>  rxfifo.rdclk,
        rdreq               =>  rxfifo.rdreq,
        wrclk               =>  rxfifo.wrclk,
        wrreq               =>  rxfifo.wrreq,
        q                   =>  rxfifo.q,
        rdempty             =>  rxfifo.rdempty,
        rdfull              =>  rxfifo.rdfull,
        rdusedw             =>  rxfifo.rdusedw,
        wrempty             =>  rxfifo.wrempty,
        wrfull              =>  rxfifo.wrfull,
        wrusedw             =>  rxfifo.wrusedw
      ) ;

    -- TX FIFO Reader
    U_fifo_reader : entity work.fifo_reader
      port map (
        clock               =>  tx_clock,
        reset               =>  reset,
        enable              =>  tx_enable,

        fifo_read           =>  txfifo.rdreq,
        fifo_empty          =>  txfifo.rdempty,
        fifo_data           =>  txfifo.q,

        out_i               =>  tx_sample_i,
        out_q               =>  tx_sample_q,
        out_valid           =>  tx_sample_valid,

        underflow_led       =>  underflow_led,
        underflow_count     =>  underflow_count,
        underflow_duration  =>  underflow_duration
      ) ;
    -- RX FIFO Writer
    U_fifo_writer : entity work.fifo_writer
      port map (
        clock               =>  rx_clock,
        reset               =>  reset,
        enable              =>  rx_enable,

        in_i                =>  rx_sample_i,
        in_q                =>  rx_sample_q,
        in_valid            =>  rx_sample_valid,

        fifo_clear          =>  rxfifo.aclr,
        fifo_write          =>  rxfifo.wrreq,
        fifo_full           =>  rxfifo.wrfull,
        fifo_data           =>  rxfifo.data,

        overflow_led        =>  overflow_led,
        overflow_count      =>  overflow_count,
        overflow_duration   =>  overflow_duration
      ) ;


    -- LMS Sample Interface
    U_lms6002d : entity work.lms6002d
      port map (
        rx_clock            =>  rx_clock,
        rx_reset            =>  reset,
        rx_enable           =>  rx_enable,

        rx_lms_data         =>  lms_rx_data,
        rx_lms_iq_sel       =>  lms_rx_iq_select,
        rx_lms_enable       =>  lms_rx_enable,

        rx_sample_i         =>  rx_native_i,
        rx_sample_q         =>  rx_native_q,
        rx_sample_valid     =>  rx_sample_valid,

        tx_clock            =>  tx_clock,
        tx_reset            =>  reset,
        tx_enable           =>  tx_enable,

        tx_sample_i         =>  tx_native_i,
        tx_sample_q         =>  tx_native_q,
        tx_sample_valid     =>  tx_sample_valid,

        tx_lms_data         =>  lms_tx_data,
        tx_lms_iq_sel       =>  lms_tx_iq_select,
        tx_lms_enable       =>  lms_tx_enable
      ) ;

    rx_sample_i <= resize(rx_native_i, rx_sample_i'length) ;
    rx_sample_q <= resize(rx_native_q, rx_sample_q'length) ;

    tx_native_i <= resize(tx_sample_i, tx_native_i'length) ;
    tx_native_q <= resize(tx_sample_q, tx_native_q'length) ;

  -- LMS Model
    U_lms6002d_model : entity work.lms6002d_model
      port map (
        resetx              =>  not(reset),

        rx_clock            =>  lms_rx_clock,
        rx_clock_out        =>  open,
        rx_data             =>  lms_rx_data,
        rx_enable           =>  lms_rx_enable,
        rx_iq_select        =>  lms_rx_iq_select,

        tx_clock            =>  lms_tx_clock,
        tx_data             =>  lms_tx_data,
        tx_enable           =>  lms_tx_enable,
        tx_iq_select        =>  lms_tx_iq_select,

        sclk                =>  '0',
        sen                 =>  '0',
        sdio                =>  '0',
        sdo                 =>  open,

        pll_out             =>  open
      ) ;

  -- TX FIFO Filler
    tx_filler : process
        variable ang        :   real  := 0.0 ;
        variable dang       :   real  := MATH_PI/100.0 ;
        variable sample_i   :   signed(15 downto 0) ;
        variable sample_q   :   signed(15 downto 0) ;
    begin
        if( reset = '1' ) then
            txfifo.data <= (others =>'0') ;
            txfifo.wrreq <= '0' ;
            wait until reset = '0' ;
        end if ;
        while true loop
            wait until rising_edge(fx3_clock) and unsigned(txfifo.wrusedw) < 512 ;
            for i in 1 to 512 loop
                sample_i := to_signed(integer(2047.0*cos(ang)),sample_i'length);
                sample_q := to_signed(integer(2047.0*sin(ang)),sample_q'length);
                txfifo.data <= std_logic_vector(sample_q & sample_i) ;
                txfifo.wrreq <= '1' ;
                nop( fx3_clock, 1 );
                ang := (ang + dang) mod MATH_2_PI ;
            end loop ;
            txfifo.wrreq <= '0' ;
            if( CHECK_UNDERFLOW ) then
                nop( fx3_clock, 3000 ) ;
            end if ;
        end loop ;
    end process ;

    -- RX FIFO Reader
    rx_reader : process
    begin
        if( reset = '1' ) then
            rxfifo.rdreq <= '0' ;
            wait until reset = '0' ;
        end if ;
        while true loop
            wait until rising_edge(fx3_clock) and unsigned(rxfifo.rdusedw) > 512 ;
            for i in 1 to 512 loop
                rxfifo.rdreq <= '1' ;
                nop( fx3_clock, 1 ) ;
                if( CHECK_OVERFLOW ) then
                    rxfifo.rdreq <= '0' ;
                    nop( fx3_clock, 5 ) ;
                end if ;
            end loop ;
            rxfifo.rdreq <= '0' ;
        end loop ;
    end process ;

    -- Testbench
    tb : process
    begin
        -- Initializing
        reset <= '1' ;
        tx_enable <= '0' ;
        rx_enable <= '0' ;
        nop( fx3_clock, 10 ) ;
        reset <= '0' ;
        nop( fx3_clock, 10 ) ;
        rx_enable <= '1' ;
        tx_enable <= '1' ;
        nop( fx3_clock, 100000 ) ;
        report "-- End of Simulation --" ;
        stop(2) ;
        wait ;
    end process ;

end architecture ;
