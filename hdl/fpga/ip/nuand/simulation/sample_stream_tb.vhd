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
    use nuand.bladerf_p.all;
    use nuand.fx3_gpif_p.all;

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
                                    wused(TX_FIFO_T_DEFAULT.wused'range)
                                );

    signal rx_sample_fifo   :   fifo_t(
                                    wdata(((NUM_MIMO_STREAMS*32)-1) downto 0),
                                    rdata( 31 downto 0), -- GPIF side is always 32 bits
                                    rused(RX_FIFO_T_DEFAULT.rused'range),
                                    wused(compute_wrusedw_high(4096,"NO") downto 0)
                                );

    signal tx_meta_fifo     :   fifo_t(
                                    wdata( 31 downto 0),
                                    rdata(127 downto 0),
                                    rused(META_FIFO_TX_T_DEFAULT.rused'range),
                                    wused(META_FIFO_TX_T_DEFAULT.wused'range)
                                );

    signal rx_meta_fifo     :   fifo_t(
                                    wdata(127 downto 0),
                                    rdata( 31 downto 0),
                                    rused(META_FIFO_RX_T_DEFAULT.rused'range),
                                    wused(META_FIFO_RX_T_DEFAULT.wused'range)
                                );

    signal tx_loopback_fifo :   loopback_fifo_t;

    -- Loopback controls
    signal tx_loopback_enabled  : std_logic := '0';

    alias rx_reset      : std_logic is reset;
    alias tx_reset      : std_logic is reset;
    alias meta_en_rx    : std_logic is meta_en;
    alias meta_en_tx    : std_logic is meta_en;
    alias rx_ts_reset   : std_logic is rx_reset;
    alias tx_ts_reset   : std_logic is tx_reset;

    signal rx_packet_control       :   packet_control_t;
    signal tx_packet_control       :   packet_control_t := PACKET_CONTROL_DEFAULT;

    signal rx_packet_ready         :   std_logic;
    signal tx_packet_ready         :   std_logic;

    -- RFIC gain tag. Layout must match fifo_writer:
    --   31:25 base index, 24 lock, 23:18/17:12/11:6/5:0 chunk deltas (signed).
    constant GAIN_TAG_CHUNKS       :   natural := 4;
    constant GAIN_TAG_DELTA_BITS   :   natural := 6;

    -- Gain indices the stimulus walks through, as CTRL_OUT bytes (bit 7 = lock).
    constant GAIN_A                :   std_logic_vector(7 downto 0) := x"28"; -- 40
    constant GAIN_B                :   std_logic_vector(7 downto 0) := x"34"; -- 52
    constant GAIN_C                :   std_logic_vector(7 downto 0) := x"B4"; -- 52 + lock

    signal rfic_ctrl_out           :   std_logic_vector(7 downto 0) := GAIN_A;
    signal rfic_ctrl_out_valid     :   std_logic := '0';
    signal gain_tags_checked        :   natural := 0;
    signal gain_tags_skipped        :   natural := 0;
    signal gain_deltas_seen         :   natural := 0;
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

            -- Packet FIFO
            packet_en            => '0',
            packet_control       => tx_packet_control,
            packet_ready         => tx_packet_ready,

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
            NUM_STREAMS            => adc_controls'length,
            ENABLE_GAIN_TAG        => true
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

            -- Packet FIFO
            packet_en              => '0',
            packet_control         => rx_packet_control,
            packet_ready           => rx_packet_ready,

            -- Samples to host via FX3
            sample_fifo_rclock     => fx3_clock,
            sample_fifo_raclr      => not rx_enable,
            sample_fifo_rreq       => rx_sample_fifo.rreq,
            sample_fifo_rdata      => rx_sample_fifo.rdata,
            sample_fifo_rempty     => rx_sample_fifo.rempty,
            sample_fifo_rfull      => rx_sample_fifo.rfull,
            sample_fifo_rused      => rx_sample_fifo.rused,

            -- Mini expansion signals
            mini_exp               => "00",

            -- RFIC CTRL_OUT, tagged into the metadata header
            rfic_ctrl_out          => rfic_ctrl_out,
            rfic_ctrl_out_valid    => rfic_ctrl_out_valid,

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
            wait until rising_edge(fx3_clock) and unsigned(rx_sample_fifo.rused) > GPIF_BUF_SIZE_SS;
            for i in 1 to GPIF_BUF_SIZE_SS loop
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

    -- Walk the RFIC gain index the way an AGC would. Atomicity of the byte is
    -- ctrl_out_xfer's job and is covered by its own testbench, so this drives
    -- rx_clock-domain values directly.
    ctrl_out_stimulus : process
    begin
        rfic_ctrl_out       <= GAIN_A;
        rfic_ctrl_out_valid <= '0';
        nop( rx_clock, 20 );
        rfic_ctrl_out_valid <= '1';

        -- Hold long enough for several whole messages at one gain, so the
        -- checker sees flat profiles (every delta zero).
        wait for 300 us;

        -- Step mid-stream: some message must show a non-zero delta.
        nop( rx_clock, 1 );
        rfic_ctrl_out <= GAIN_B;
        wait for 300 us;

        -- Same index, gain lock asserted.
        nop( rx_clock, 1 );
        rfic_ctrl_out <= GAIN_C;
        wait for 300 us;

        nop( rx_clock, 1 );
        rfic_ctrl_out <= GAIN_A;
        wait;
    end process;

    -- Drain the RX metadata FIFO and check the gain profile in each header.
    --
    -- The meta FIFO is 128 bits wide on the write side and 32 on the read side,
    -- so each header comes out as four words with the reserved word first. Reads
    -- start from empty, so counting words keeps us aligned to that boundary.
    -- LPM_SHOWAHEAD is ON, meaning rdata already presents the head of the queue
    -- and rreq pops it -- so the word consumed at an edge is the rdata visible
    -- during the cycle in which rreq is high.
    rx_meta_reader : process( fx3_clock, reset )
        variable word_idx : natural range 0 to 3 := 0;
        variable tag      : std_logic_vector(31 downto 0);
        variable base     : integer;
        variable delta    : integer;
        variable absolute : integer;
        variable nonzero  : boolean;
    begin
        if( reset = '1' ) then
            rx_meta_fifo.rreq <= '0';
            word_idx          := 0;
        elsif( rising_edge(fx3_clock) ) then
            if( rx_meta_fifo.rreq = '1' ) then
                if( word_idx = 0 ) then
                    tag  := rx_meta_fifo.rdata;
                    base := to_integer(unsigned(tag(31 downto 25)));

                    if( base = 0 ) then
                        -- The cross-domain byte had not produced a settled value
                        -- when this message started, so fifo_writer held its
                        -- post-reset base. The stimulus never drives index 0, so
                        -- this is unambiguous. Only expected at start of stream.
                        gain_tags_skipped <= gain_tags_skipped + 1;
                    else
                        -- The stimulus only ever drives indices 40 and 52.
                        assert( base = 40 or base = 52 )
                            report "gain tag base is not an index the stimulus " &
                                   "drove: " & integer'image(base) & " (word " &
                                   to_hstring(tag) & ")"
                            severity failure;

                        nonzero := false;
                        for i in 0 to GAIN_TAG_CHUNKS-1 loop
                            delta := to_integer(signed(
                                tag((GAIN_TAG_CHUNKS-1-i)*GAIN_TAG_DELTA_BITS +
                                    GAIN_TAG_DELTA_BITS-1 downto
                                    (GAIN_TAG_CHUNKS-1-i)*GAIN_TAG_DELTA_BITS)));
                            absolute := base + delta;

                            if( delta /= 0 ) then
                                nonzero := true;
                            end if;

                            -- Every reconstructed index must still be one the
                            -- stimulus drove; a mis-packed or mis-signed field
                            -- shows up here.
                            assert( absolute = 40 or absolute = 52 )
                                report "chunk " & integer'image(i) &
                                       " of gain tag " & to_hstring(tag) &
                                       " reconstructs to " &
                                       integer'image(absolute) &
                                       ", which the stimulus never drove"
                                severity failure;
                        end loop;

                        if( nonzero ) then
                            gain_deltas_seen <= gain_deltas_seen + 1;
                        end if;

                        gain_tags_checked <= gain_tags_checked + 1;
                    end if;
                end if;

                if( word_idx = 3 ) then
                    word_idx := 0;
                else
                    word_idx := word_idx + 1;
                end if;
            end if;

            rx_meta_fifo.rreq <= not rx_meta_fifo.rempty;
        end if;
    end process;

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
        -- Guard against the gain tag checker silently never running
        assert( gain_tags_checked > 0 )
            report "no RX metadata headers were checked for a gain tag"
            severity failure ;
        -- The base alone would pass even if the delta fields were dead, so
        -- require that at least one message straddled the gain step.
        assert( gain_deltas_seen > 0 )
            report "no gain tag carried a non-zero chunk delta, so the delta " &
                   "path is untested"
            severity failure ;
        report "-- Gain tags checked: " & integer'image(gain_tags_checked) &
               ", with deltas: " & integer'image(gain_deltas_seen) &
               ", skipped (pre-valid): " & integer'image(gain_tags_skipped) ;
        report "-- End of Simulation --" ;
        stop(2) ;
        wait ;
    end process ;

end architecture ;
