-- Copyright (c) 2013-2017 Nuand LLC
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;

library nuand ;
    use nuand.util.all ;
    use nuand.fifo_readwrite_p.all;
    use nuand.common_dcfifo_p.all;

library std ;
    use std.env.all ;
    use std.textio.all ;

entity sample_stream_tb is
    generic (
        -- For bladeRF (SISO):
        NUM_MIMO_STREAMS            : natural := 1;
        FIFO_READER_READ_THROTTLE   : natural := 1;

        -- For bladeRF2 (2x2 MIMO):
        --NUM_MIMO_STREAMS          : natural := 2;
        --FIFO_READER_READ_THROTTLE : natural := 0;

        ENABLE_CHANNEL_0            : std_logic := '1';
        ENABLE_CHANNEL_1            : std_logic := '1'
    );
end entity ;

architecture arch of sample_stream_tb is

    -- Checking constants
    constant CHECK_OVERFLOW     :   boolean     := false ;
    constant CHECK_UNDERFLOW    :   boolean     := false ;

    -- Clock half periods
    constant FX3_HALF_PERIOD    :   time        := 1.0/(100.0e6)/2.0*1 sec ;
    constant TX_HALF_PERIOD     :   time        := 1.0/(9.0e6)/2.0*1 sec ;
    constant RX_HALF_PERIOD     :   time        := 1.0/(9.0e6)/2.0*1 sec ;

    signal dac_controls         :   sample_controls_t(0 to NUM_MIMO_STREAMS-1)  := (others => SAMPLE_CONTROL_DISABLE);
    signal dac_streams          :   sample_streams_t(dac_controls'range)        := (others => ZERO_SAMPLE);
    signal adc_controls         :   sample_controls_t(0 to NUM_MIMO_STREAMS-1)  := (others => SAMPLE_CONTROL_DISABLE);
    signal adc_streams          :   sample_streams_t(adc_controls'range)        := (others => ZERO_SAMPLE);

    -- Clocks
    signal fx3_clock            :   std_logic   := '1' ;
    signal tx_clock             :   std_logic   := '1' ;
    signal rx_clock             :   std_logic   := '1' ;

    signal reset                :   std_logic   := '1' ;

    -- Configuration
    signal usb_speed            :   std_logic ;
    signal meta_en              :   std_logic ;
    signal rx_timestamp         :   unsigned(63 downto 0)       := (others =>'0') ;
    signal tx_timestamp         :   unsigned(63 downto 0)       := (others =>'0') ;

    -- TX Signalling
    signal tx_enable            :   std_logic ;
    signal tx_native_i          :   signed(11 downto 0) ;
    signal tx_native_q          :   signed(11 downto 0) ;
    signal tx_sample_i          :   signed(15 downto 0) ;
    signal tx_sample_q          :   signed(15 downto 0) ;
    signal tx_sample_valid      :   std_logic ;

    signal tx_samples           :   sample_streams_t(0 to NUM_MIMO_STREAMS-1) := (others => ZERO_SAMPLE);

    -- RX Signalling
    signal rx_enable            :   std_logic ;
    signal rx_native_i          :   signed(11 downto 0) ;
    signal rx_native_q          :   signed(11 downto 0) ;
    signal rx_sample_i          :   signed(15 downto 0) ;
    signal rx_sample_q          :   signed(15 downto 0) ;
    signal rx_sample_valid      :   std_logic ;

    signal rx_samples           :   sample_streams_t(0 to NUM_MIMO_STREAMS-1) := (others => ZERO_SAMPLE);

    -- Underflow
    signal underflow_led        :   std_logic ;
    signal underflow_count      :   unsigned(63 downto 0) ;
    signal underflow_duration   :   unsigned(15 downto 0) := to_unsigned(20, 16) ;

    -- Overflow
    signal overflow_led         :   std_logic ;
    signal overflow_count       :   unsigned(63 downto 0) ;
    signal overflow_duration    :   unsigned(15 downto 0) := to_unsigned(20, 16) ;

    -- FIFO type
    type fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector;
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector;

        rclock  :   std_logic;
        rdata   :   std_logic_vector;
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector;
    end record;

    -- FIFOs
    signal tx_sample_fifo   :   fifo_t(
                                    wdata( 31 downto 0), -- GPIF side is always 32 bits
                                    rdata(((NUM_MIMO_STREAMS*32)-1) downto 0),
                                    rused(compute_rdusedw_high(4096,32,(NUM_MIMO_STREAMS*32),"NO") downto 0),
                                    wused(compute_wrusedw_high(4096,"NO") downto 0)
                                );

    signal rx_sample_fifo   :   fifo_t(
                                    wdata(((NUM_MIMO_STREAMS*32)-1) downto 0),
                                    rdata( 31 downto 0), -- GPIF side is always 32 bits
                                    rused(compute_rdusedw_high(4096,(NUM_MIMO_STREAMS*32),32,"NO") downto 0),
                                    wused(compute_wrusedw_high(4096,"NO") downto 0)
                                );

    signal tx_meta_fifo     :   fifo_t(
                                    wdata( 31 downto 0),
                                    rdata(127 downto 0),
                                    rused(  2 downto 0),
                                    wused(  4 downto 0)
                                );

    signal rx_meta_fifo     :   fifo_t(
                                    wdata(127 downto 0),
                                    rdata( 31 downto 0),
                                    rused(  6 downto 0),
                                    wused(  4 downto 0)
                                );

    signal tx_loopback_fifo :   fifo_t(
                                    wdata(63 downto 0),
                                    rdata(63 downto 0),
                                    rused(compute_rdusedw_high(2048,64,64,"OFF") downto 0),
                                    wused(compute_wrusedw_high(2048,"OFF") downto 0)
                                );

    -- Loopback controls
    signal tx_loopback_enabled  : std_logic := '0';

    alias rx_reset      : std_logic is reset;
    alias tx_reset      : std_logic is reset;
    alias meta_en_rx    : std_logic is meta_en;
    alias meta_en_tx    : std_logic is meta_en;
    alias rx_ts_reset   : std_logic is rx_reset;
    alias tx_ts_reset   : std_logic is tx_reset;

begin

    usb_speed <= '0' ;
    meta_en <= '1' ;

    increment_tx_ts : process(tx_clock)
        variable ping : boolean := true ;
    begin
        if( rising_edge(tx_clock) ) then
            ping := not ping ;
            if( ping = true ) then
                tx_timestamp <= tx_timestamp + 1 ;
            end if ;
        end if ;
    end process ;

    increment_rx_ts : process(rx_clock)
        variable ping : boolean := true ;
    begin
        if( rising_edge(rx_clock) ) then
            ping := not ping ;
            if( ping = true ) then
                rx_timestamp <= rx_timestamp + 1 ;
            end if ;
        end if ;
    end process ;

    -- Clock creation
    fx3_clock   <= not fx3_clock after FX3_HALF_PERIOD ;
    tx_clock    <= not tx_clock  after TX_HALF_PERIOD ;
    rx_clock    <= not rx_clock  after RX_HALF_PERIOD ;

    -- TX Submodule
    U_tx : entity work.tx
        generic map (
            NUM_STREAMS          => dac_controls'length
        )
        port map (
            tx_reset             => tx_reset,
            tx_clock             => tx_clock,
            tx_enable            => tx_enable,

            meta_en              => meta_en_tx,
            timestamp_reset      => tx_ts_reset,
            usb_speed            => usb_speed,
            tx_underflow_led     => underflow_led,
            tx_timestamp         => tx_timestamp,

            -- Triggering
            trigger_arm          => '0',
            trigger_fire         => '0',
            trigger_master       => '0',
            trigger_line         => open,

            -- Samples from host via FX3
            sample_fifo_wclock   => fx3_clock,
            sample_fifo_wreq     => tx_sample_fifo.wreq,
            sample_fifo_wdata    => tx_sample_fifo.wdata,
            sample_fifo_wempty   => tx_sample_fifo.wempty,
            sample_fifo_wfull    => tx_sample_fifo.wfull,
            sample_fifo_wused    => tx_sample_fifo.wused,

            -- Metadata from host via FX3
            meta_fifo_wclock     => fx3_clock,
            meta_fifo_wreq       => tx_meta_fifo.wreq,
            meta_fifo_wdata      => tx_meta_fifo.wdata,
            meta_fifo_wempty     => tx_meta_fifo.wempty,
            meta_fifo_wfull      => tx_meta_fifo.wfull,
            meta_fifo_wused      => tx_meta_fifo.wused,

            -- Digital Loopback Interface
            loopback_enabled     => tx_loopback_enabled,
            loopback_fifo_wdata  => tx_loopback_fifo.wdata,
            loopback_fifo_wreq   => tx_loopback_fifo.wreq,
            loopback_fifo_wfull  => tx_loopback_fifo.wfull,
            loopback_fifo_wused  => tx_loopback_fifo.wused,

            -- RFFE Interface
            dac_controls         => dac_controls,
            dac_streams          => dac_streams
        );

    -- RX Submodule
    U_rx : entity work.rx
        generic map (
            NUM_STREAMS            => adc_controls'length
        )
        port map (
            rx_reset               => rx_reset,
            rx_clock               => rx_clock,
            rx_enable              => rx_enable,

            meta_en                => meta_en_rx,
            timestamp_reset        => rx_ts_reset,
            usb_speed              => usb_speed,
            rx_mux_sel             => to_unsigned(4, 3), -- digital loopback
            rx_overflow_led        => overflow_led,
            rx_timestamp           => rx_timestamp,

            -- Triggering
            trigger_arm            => '0',
            trigger_fire           => '0',
            trigger_master         => '0',
            trigger_line           => open,

            -- Samples to host via FX3
            sample_fifo_rclock     => fx3_clock,
            sample_fifo_raclr      => not rx_enable,
            sample_fifo_rreq       => rx_sample_fifo.rreq,
            sample_fifo_rdata      => rx_sample_fifo.rdata,
            sample_fifo_rempty     => rx_sample_fifo.rempty,
            sample_fifo_rfull      => rx_sample_fifo.rfull,
            sample_fifo_rused      => rx_sample_fifo.rused,

            -- Metadata to host via FX3
            meta_fifo_rclock       => fx3_clock,
            meta_fifo_raclr        => not rx_enable,
            meta_fifo_rreq         => rx_meta_fifo.rreq,
            meta_fifo_rdata        => rx_meta_fifo.rdata,
            meta_fifo_rempty       => rx_meta_fifo.rempty,
            meta_fifo_rfull        => rx_meta_fifo.rfull,
            meta_fifo_rused        => rx_meta_fifo.rused,

            -- Digital Loopback Interface
            loopback_fifo_wenabled => tx_loopback_enabled,
            loopback_fifo_wreset   => tx_reset,
            loopback_fifo_wclock   => tx_clock,
            loopback_fifo_wdata    => tx_loopback_fifo.wdata,
            loopback_fifo_wreq     => tx_loopback_fifo.wreq,
            loopback_fifo_wfull    => tx_loopback_fifo.wfull,
            loopback_fifo_wused    => tx_loopback_fifo.wused,

            -- RFFE Interface
            adc_controls           => adc_controls,
            adc_streams            => adc_streams
        );

    gen_dac_controls : if( NUM_MIMO_STREAMS > 1 ) generate
        -- The TX side of the AD9361 HDL is a FIFO pull interface
        -- that expects a readahead FIFO. It toggles the data request
        -- signal every other cycle. This behavior is mimicked here.
        process( tx_clock, reset )
        begin
            if( reset = '1' ) then
                dac_controls <= (
                    0 => (enable => ENABLE_CHANNEL_0, data_req => '1'),
                    1 => (enable => ENABLE_CHANNEL_1, data_req => '1')
                );
            elsif( rising_edge(tx_clock) ) then
                for i in dac_controls'range loop
                    dac_controls(i) <= (
                        enable   =>     dac_controls(i).enable,
                        data_req => not dac_controls(i).data_req );
                end loop;
            end if;
        end process;
    else generate
        -- The LMS6 HDL is not nearly as complicated (or featureful)
        -- as the AD9361. For LMS6, we just push samples directly to
        -- the device, so the data request line can stay asserted.
        dac_controls <= (others => SAMPLE_CONTROL_ENABLE);
    end generate;

    gen_adc_controls : if( NUM_MIMO_STREAMS > 1 ) generate
        process( rx_clock, reset )
        begin
            if( reset = '1' ) then
                adc_controls <= (
                    0 => (enable => ENABLE_CHANNEL_0, data_req => '1'),
                    1 => (enable => ENABLE_CHANNEL_1, data_req => '1')
                );
            elsif( rising_edge(rx_clock) ) then
                for i in adc_controls'range loop
                    adc_controls(i) <= (
                        enable   =>     adc_controls(i).enable,
                        data_req => not adc_controls(i).data_req );
                end loop;
            end if;
        end process;
    else generate
        adc_controls <= (others => SAMPLE_CONTROL_ENABLE);
    end generate;

    -- TX FIFO Filler
    tx_filler : process
        variable ang        :   real  := 0.0 ;
        variable dang       :   real  := MATH_PI/100.0 ;
        variable sample_i   :   signed(15 downto 0) ;
        variable sample_q   :   signed(15 downto 0) ;
        variable ts         :   integer ;
    begin
        if( reset = '1' ) then
            tx_sample_fifo.wdata <= (others =>'0') ;
            tx_sample_fifo.wreq <= '0' ;
            wait until reset = '0' ;
        end if ;
        for i in 1 to 100 loop
            wait until rising_edge(fx3_clock) ;
        end loop ;
        for j in 1 to 5 loop
            ts := 16#1000# + (j-1)*10000 ;
            for i in 1 to 5 loop
                tx_meta_fifo.wdata <= (others =>'0') ;
                wait until rising_edge(fx3_clock) and unsigned(tx_sample_fifo.wused) < 1024 ;
                tx_meta_fifo.wdata <= x"12345678" ;
                tx_meta_fifo.wreq <= '1' ;
                wait until rising_edge(fx3_clock) ;
                tx_meta_fifo.wdata <= std_logic_vector(to_unsigned(ts,tx_meta_fifo.wdata'length)) ;
                ts := ts + 1020 ;
                wait until rising_edge(fx3_clock) ;
                tx_meta_fifo.wdata <= x"00000000" ;
                wait until rising_edge(fx3_clock) ;
                tx_meta_fifo.wdata <= x"00000000" ;
                wait until rising_edge(fx3_clock) ;
                tx_meta_fifo.wreq <= '0' ;
                tx_meta_fifo.wdata <= (others =>'0') ;
                for r in 1 to 1020 loop
                    if( r = 1 or r = 1020 ) then
                        sample_i := (others =>'0') ;
                        sample_q := (others =>'0') ;
                    else
                        sample_i := to_signed(2047, sample_i'length) ;
                        sample_q := to_signed(2047, sample_q'length) ;
                    end if ;
                    sample_i := to_signed(integer(2047.0*cos(ang)),sample_i'length);
                    sample_q := to_signed(integer(2047.0*sin(ang)),sample_q'length);
                    tx_sample_fifo.wdata <= std_logic_vector(sample_q & sample_i) after 0.1 ns ;
                    tx_sample_fifo.wreq <= '1' after 0.1 ns ;
                    nop( fx3_clock, 1 );
                    tx_sample_fifo.wreq <= '0' after 0.1 ns ;
                    ang := (ang + dang) mod MATH_2_PI ;
                end loop ;
                if( CHECK_UNDERFLOW ) then
                    nop( fx3_clock, 3000 ) ;
                end if ;
                tx_sample_fifo.wdata <= (others =>'X') after 0.1 ns ;
            end loop ;
            nop(fx3_clock, 100000) ;
        end loop ;
        wait ;
    end process ;

    -- RX FIFO Reader
    rx_reader : process
    begin
        if( reset = '1' ) then
            rx_sample_fifo.rreq <= '0' ;
            wait until reset = '0' ;
        end if ;
        while true loop
            wait until rising_edge(fx3_clock) and unsigned(rx_sample_fifo.rused) > 512 ;
            for i in 1 to 512 loop
                rx_sample_fifo.rreq <= '1' ;
                nop( fx3_clock, 1 ) ;
                if( CHECK_OVERFLOW ) then
                    rx_sample_fifo.rreq <= '0' ;
                    nop( fx3_clock, 5 ) ;
                end if ;
            end loop ;
            rx_sample_fifo.rreq <= '0' ;
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
        nop( fx3_clock, 2000000 ) ;
        report "-- End of Simulation --" ;
        stop(2) ;
        wait ;
    end process ;

end architecture ;
