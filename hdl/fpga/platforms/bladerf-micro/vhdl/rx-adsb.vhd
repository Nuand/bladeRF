-- Copyright (c) 2017 Nuand LLC
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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

library work;
    use work.bladerf_p.all;
    use work.fifo_readwrite_p.all;

entity rx is
    generic (
        NUM_STREAMS          : natural := 2
    );
    port (
        rx_reset               : in    std_logic;
        rx_clock               : in    std_logic;
        rx_enable              : in    std_logic;

        meta_en                : in    std_logic := '0';
        timestamp_reset        : out   std_logic := '1';
        usb_speed              : in    std_logic;
        rx_mux_sel             : in    unsigned;
        rx_overflow_led        : out   std_logic := '1';
        rx_timestamp           : in    unsigned(63 downto 0);

        -- Triggering
        trigger_arm            : in    std_logic;
        trigger_fire           : in    std_logic;
        trigger_master         : in    std_logic;
        trigger_line           : inout std_logic; -- this is not good, should be in/out/oe

        -- Samples to host via FX3
        sample_fifo_rclock     : in    std_logic;
        sample_fifo_raclr      : in    std_logic;
        sample_fifo_rreq       : in    std_logic;
        sample_fifo_rdata      : out   std_logic_vector(ADSB_FIFO_T_DEFAULT.rdata'range);
        sample_fifo_rempty     : out   std_logic;
        sample_fifo_rfull      : out   std_logic;
        sample_fifo_rused      : out   std_logic_vector(ADSB_FIFO_T_DEFAULT.rused'range);

        -- Mini expansion signals
        mini_exp               : in    std_logic_vector(1 downto 0);

        -- Metadata to host via FX3
        meta_fifo_rclock       : in    std_logic;
        meta_fifo_raclr        : in    std_logic;
        meta_fifo_rreq         : in    std_logic;
        meta_fifo_rdata        : out   std_logic_vector(META_FIFO_RX_T_DEFAULT.rdata'range);
        meta_fifo_rempty       : out   std_logic;
        meta_fifo_rfull        : out   std_logic;
        meta_fifo_rused        : out   std_logic_vector(META_FIFO_RX_T_DEFAULT.rused'range);

        -- Digital Loopback Interface
        loopback_fifo_wenabled : out   std_logic;
        loopback_fifo_wreset   : in    std_logic;
        loopback_fifo_wclock   : in    std_logic;
        loopback_fifo_wdata    : in    std_logic_vector(LOOPBACK_FIFO_T_DEFAULT.wdata'range);
        loopback_fifo_wreq     : in    std_logic;
        loopback_fifo_wfull    : out   std_logic;
        loopback_fifo_wused    : out   std_logic_vector(LOOPBACK_FIFO_T_DEFAULT.wused'range);

        -- ADSB
        adsb_data              : in    std_logic_vector(127 downto 0);
        adsb_valid             : in    std_logic;

        -- RFFE Interface
        adc_controls           : in    sample_controls_t(0 to NUM_STREAMS-1) := (others => SAMPLE_CONTROL_DISABLE);
        adc_streams            : in    sample_streams_t(0 to NUM_STREAMS-1)  := (others => ZERO_SAMPLE)
    );
end entity;

architecture arch of rx is

    -- Can be set from libbladeRF using bladerf_set_rx_mux()
    type rx_mux_mode_t is (
        RX_MUX_NORMAL,
        RX_MUX_12BIT_COUNTER,
        RX_MUX_32BIT_COUNTER,
        RX_MUX_ENTROPY,
        RX_MUX_DIGITAL_LOOPBACK
    );

    signal rx_mux_mode              : rx_mux_mode_t       := RX_MUX_NORMAL;

    signal sample_fifo              : adsb_fifo_t         := ADSB_FIFO_T_DEFAULT;
    signal loopback_fifo            : loopback_fifo_t     := LOOPBACK_FIFO_T_DEFAULT;
    signal meta_fifo                : meta_fifo_rx_t      := META_FIFO_RX_T_DEFAULT;

    signal loopback_streams         : sample_streams_t(adc_streams'range) := (others => ZERO_SAMPLE);
    signal loopback_enabled         : std_logic           := '0';
    signal loopback_fifo_wenabled_i : std_logic           := '0';

    signal rx_gen_mode              : std_logic;
    signal rx_gen_i                 : signed(15 downto 0);
    signal rx_gen_q                 : signed(15 downto 0);
    signal rx_gen_valid             : std_logic;

    signal mux_streams              : sample_streams_t(adc_streams'range) := (others => ZERO_SAMPLE);

    signal trigger_signal_out       : std_logic;
    signal trigger_signal_out_sync  : std_logic;

begin

    rx_mux_mode            <= rx_mux_mode_t'val(to_integer(rx_mux_sel));
    loopback_fifo_wenabled <= loopback_fifo_wenabled_i;

    set_timestamp_reset : process(rx_clock, rx_reset)
    begin
        if( rx_reset = '1' ) then
            timestamp_reset <= '1';
        elsif( rising_edge(rx_clock) ) then
            if( meta_en = '1' ) then
                timestamp_reset <= '0';
            else
                timestamp_reset <= '1';
            end if;
        end if;
    end process;

    sample_fifo.wdata  <= adsb_data;
    sample_fifo.wreq   <= adsb_valid;

    -- RX sample FIFO
    sample_fifo.aclr   <= sample_fifo_raclr;
    sample_fifo.wclock <= rx_clock;
    U_rx_sample_fifo : entity work.adsbfifo
        port map (
            aclr                => sample_fifo.aclr,

            wrclk               => sample_fifo.wclock,
            wrreq               => sample_fifo.wreq,
            data                => sample_fifo.wdata,
            wrempty             => sample_fifo.wempty,
            wrfull              => sample_fifo.wfull,
            wrusedw             => sample_fifo.wused,

            rdclk               => sample_fifo_rclock,
            rdreq               => sample_fifo_rreq,
            q                   => sample_fifo_rdata,
            rdempty             => sample_fifo_rempty,
            rdfull              => sample_fifo_rfull,
            rdusedw             => sample_fifo_rused
        );


    -- RX meta FIFO
    meta_fifo.aclr   <= meta_fifo_raclr;
    meta_fifo.wclock <= rx_clock;
    U_rx_meta_fifo : entity work.rx_meta_fifo
        generic map (
            LPM_NUMWORDS        => 2**(meta_fifo.wused'length)
        ) port map (
            aclr                => meta_fifo.aclr,

            wrclk               => meta_fifo.wclock,
            wrreq               => meta_fifo.wreq,
            data                => meta_fifo.wdata,
            wrempty             => meta_fifo.wempty,
            wrfull              => meta_fifo.wfull,
            wrusedw             => meta_fifo.wused,

            rdclk               => meta_fifo_rclock,
            rdreq               => meta_fifo_rreq,
            q                   => meta_fifo_rdata,
            rdempty             => meta_fifo_rempty,
            rdfull              => meta_fifo_rfull,
            rdusedw             => meta_fifo_rused
        );


    -- RX loopback FIFO
    loopback_fifo.aclr   <= '1' when ( (loopback_fifo_wreset = '1') or (loopback_fifo_wenabled_i = '0') ) else '0';
    loopback_fifo.rclock <= rx_clock;

    U_rx_loopback_fifo : entity work.lb_fifo
        generic map (
            LPM_NUMWORDS        => 2**(loopback_fifo.rused'length)
        )
        port map (
            aclr                => loopback_fifo.aclr,

            wrclk               => loopback_fifo_wclock,
            wrreq               => loopback_fifo_wreq,
            data                => loopback_fifo_wdata,
            wrempty             => open,
            wrfull              => loopback_fifo_wfull,
            wrusedw             => loopback_fifo_wused,

            rdclk               => loopback_fifo.rclock,
            rdreq               => loopback_fifo.rreq,
            q                   => loopback_fifo.rdata,
            rdempty             => loopback_fifo.rempty,
            rdfull              => loopback_fifo.rfull,
            rdusedw             => loopback_fifo.rused
        );


    -- Sample bridge
    U_fifo_writer : entity work.fifo_writer
        generic map (
            NUM_STREAMS           => NUM_STREAMS,
            FIFO_USEDW_WIDTH      => sample_fifo.wused'length,
            FIFO_DATA_WIDTH       => sample_fifo.wdata'length,
            META_FIFO_USEDW_WIDTH => meta_fifo.wused'length,
            META_FIFO_DATA_WIDTH  => meta_fifo.wdata'length
        )
        port map (
            clock               =>  rx_clock,
            reset               =>  rx_reset,
            enable              =>  rx_enable,

            usb_speed           =>  usb_speed,
            meta_en             =>  meta_en,
            timestamp           =>  rx_timestamp,
            mini_exp            =>  mini_exp,

            fifo_full           =>  sample_fifo.wfull,
            fifo_usedw          =>  sample_fifo.wused,
            fifo_data           =>  open,
            fifo_write          =>  open,

            meta_fifo_full      =>  meta_fifo.wfull,
            meta_fifo_usedw     =>  meta_fifo.wused,
            meta_fifo_data      =>  meta_fifo.wdata,
            meta_fifo_write     =>  meta_fifo.wreq,

            in_sample_controls  =>  adc_controls,
            in_samples          =>  mux_streams,

            overflow_led        =>  rx_overflow_led,
            overflow_count      =>  open,
            overflow_duration   =>  x"ffff"
        );


    loopback_fifo_control : process( rx_reset, loopback_fifo.rclock )
        variable offset     : natural range 0 to loopback_fifo.rdata'length;
        variable remaining  : natural range 0 to loopback_fifo.rdata'length/32;
        variable loopdata   : std_logic_vector(loopback_fifo.rdata'range);
    begin
        if( rx_reset = '1' ) then
            loopback_enabled    <= '0';
            loopback_fifo.rreq  <= '0';
            loopback_streams    <= (others => ZERO_SAMPLE);
            remaining           := 0;
        elsif( rising_edge(loopback_fifo.rclock) ) then

            -- Is loopback enabled?
            if( rx_mux_mode = RX_MUX_DIGITAL_LOOPBACK and rx_enable = '1' ) then
                loopback_enabled    <= '1';
            else
                loopback_enabled    <= '0';
                loopback_fifo.rreq  <= '0';
                remaining           := 0;
            end if;

            -- Clear data valids
            for i in loopback_streams'range loop
                loopback_streams(i).data_v <= '0';
            end loop;

            -- Handle loopback FIFO
            if( loopback_fifo.rreq = '1' ) then
                -- We have fresh data!
                loopback_fifo.rreq <= '0';
                loopdata    := loopback_fifo.rdata;
                remaining   := loopback_fifo.rdata'length/32;
            elsif( remaining = 0 and sample_fifo.wfull = '0' and loopback_fifo.rempty = '0' ) then
                -- Read more from the FIFO if we can
                loopback_fifo.rreq <= '1';
            end if;

            -- Do the loopback
            for i in loopback_streams'range loop

                offset := loopdata'length - (remaining*32);

                if( adc_controls(i).enable = '1' and remaining > 0 ) then
                    loopback_streams(i).data_i <=
                        resize(signed(loopdata(offset+11 downto offset+0)),
                                loopback_streams(i).data_i'length);

                    loopback_streams(i).data_q <=
                        resize(signed(loopdata(offset+27 downto offset+16)),
                                loopback_streams(i).data_q'length);

                    loopback_streams(i).data_v <= '1';

                    -- Shift our loopdata index
                    if( remaining > 0 ) then
                        remaining := remaining - 1;
                    end if;
                end if;
            end loop;
        end if;
    end process;


    U_rx_siggen : entity work.signal_generator
        port map (
            clock           =>  rx_clock,
            reset           =>  rx_reset,
            enable          =>  rx_enable,

            mode            =>  rx_gen_mode,

            sample_i        =>  rx_gen_i,
            sample_q        =>  rx_gen_q,
            sample_valid    =>  rx_gen_valid
        );


    rx_mux : process(rx_reset, rx_clock)
    begin
        if( rx_reset = '1' ) then
            mux_streams  <= (others => ZERO_SAMPLE);
            rx_gen_mode  <= '0';
        elsif( rising_edge(rx_clock) ) then
            case rx_mux_mode is
                when RX_MUX_NORMAL =>
                    mux_streams <= adc_streams;
                    if( trigger_signal_out_sync = '0' ) then
                        for i in mux_streams'range loop
                            mux_streams(i).data_v <= '0';
                        end loop;
                    end if;
                when RX_MUX_12BIT_COUNTER | RX_MUX_32BIT_COUNTER =>
                    for i in mux_streams'range loop
                        mux_streams(i).data_i <= rx_gen_i;
                        mux_streams(i).data_q <= rx_gen_q;
                        mux_streams(i).data_v <= rx_gen_valid;
                    end loop;

                    if( rx_mux_mode = RX_MUX_32BIT_COUNTER ) then
                        rx_gen_mode <= '1';
                    else
                        rx_gen_mode <= '0';
                    end if;
                when RX_MUX_ENTROPY =>
                    -- Not yet implemented
                    mux_streams  <= (others => ZERO_SAMPLE);
                when RX_MUX_DIGITAL_LOOPBACK =>
                    mux_streams  <= loopback_streams;
                when others =>
                    mux_streams  <= (others => ZERO_SAMPLE);
            end case;
        end if;
    end process;


    -- RX Trigger
    rxtrig : entity work.trigger(async)
        generic map (
            DEFAULT_OUTPUT  => '0'
        )
        port map (
            armed           => trigger_arm,       -- in  sl
            fired           => trigger_fire,      -- in  sl
            master          => trigger_master,    -- in  sl
            trigger_in      => trigger_line,      -- in  sl
            trigger_out     => trigger_line,      -- out sl
            signal_in       => rx_enable,         -- in  sl
            signal_out      => trigger_signal_out -- out sl
        );


    U_reset_sync_loopback : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL         => '0',
            OUTPUT_LEVEL        => '0'
        )
        port map (
            clock               =>  loopback_fifo_wclock,
            async               =>  loopback_enabled,
            sync                =>  loopback_fifo_wenabled_i
        );


    U_sync_rxtrig_signal_out : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        )
        port map (
            reset       =>  rx_reset,
            clock       =>  rx_clock,
            async       =>  trigger_signal_out,
            sync        =>  trigger_signal_out_sync
        );

end architecture;
