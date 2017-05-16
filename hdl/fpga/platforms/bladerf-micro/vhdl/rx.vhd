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

entity rx is
    port(
        rx_reset               : in    std_logic;
        rx_clock               : in    std_logic;
        rx_enable              : in    std_logic;

        meta_en                : in    std_logic := '0';
        timestamp_reset        : out   std_logic := '1';
        usb_speed              : in    std_logic;
        rx_mux_sel             : in    unsigned;
        rx_overflow_led        : out   std_logic := '1';
        rx_timestamp           : in    unsigned(63 downto 0);
        mimo_channel_sel       : in    std_logic := '0'; -- temporary

        -- Triggering
        trigger_arm            : in    std_logic;
        trigger_fire           : in    std_logic;
        trigger_master         : in    std_logic;
        trigger_line           : inout std_logic; -- this is not good, should be in/out/oe

        -- Samples to host via FX3
        sample_fifo_rclock     : in    std_logic;
        sample_fifo_raclr      : in    std_logic;
        sample_fifo_rreq       : in    std_logic;
        sample_fifo_rdata      : out   std_logic_vector(FIFO_T_DEFAULT.rdata'range);
        sample_fifo_rempty     : out   std_logic;
        sample_fifo_rfull      : out   std_logic;
        sample_fifo_rused      : out   std_logic_vector(FIFO_T_DEFAULT.rused'range);

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
        loopback_fifo_wdata    : in    std_logic_vector(FIFO_T_DEFAULT.wdata'range);
        loopback_fifo_wreq     : in    std_logic;

        -- RFFE Interface
        adc_0_enable           : in    std_logic;
        adc_0_valid            : in    std_logic;
        adc_0_i                : in    std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(0).dac.i.data'range);
        adc_0_q                : in    std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(0).dac.q.data'range);
        adc_1_enable           : in    std_logic;
        adc_1_valid            : in    std_logic;
        adc_1_i                : in    std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(1).dac.i.data'range);
        adc_1_q                : in    std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(1).dac.q.data'range)
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

    signal sample_fifo              : fifo_t              := FIFO_T_DEFAULT;
    signal loopback_fifo            : fifo_t              := FIFO_T_DEFAULT;
    signal meta_fifo                : meta_fifo_rx_t      := META_FIFO_RX_T_DEFAULT;

    signal loopback_i               : signed(15 downto 0) := (others =>'0');
    signal loopback_q               : signed(15 downto 0) := (others =>'0');
    signal loopback_valid           : std_logic           := '0';
    signal loopback_enabled         : std_logic           := '0';
    signal loopback_fifo_wenabled_i : std_logic           := '0';

    signal rx_sample_i              : signed(15 downto 0);
    signal rx_sample_q              : signed(15 downto 0);
    signal rx_sample_valid          : std_logic;

    signal rx_gen_mode              : std_logic;
    signal rx_gen_i                 : signed(15 downto 0);
    signal rx_gen_q                 : signed(15 downto 0);
    signal rx_gen_valid             : std_logic;

    signal rx_mux_i                 : signed(15 downto 0);
    signal rx_mux_q                 : signed(15 downto 0);
    signal rx_mux_valid             : std_logic;

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


    -- RX sample FIFO
    sample_fifo.aclr   <= sample_fifo_raclr;
    sample_fifo.wclock <= rx_clock;
    U_rx_sample_fifo : entity work.rx_fifo
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
        port map (
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
    U_rx_loopback_fifo : entity work.rx_fifo
        port map (
            aclr                => loopback_fifo.aclr,

            wrclk               => loopback_fifo_wclock,
            wrreq               => loopback_fifo_wreq,
            data                => loopback_fifo_wdata,
            wrempty             => open,
            wrfull              => open,
            wrusedw             => open,

            rdclk               => loopback_fifo.rclock,
            rdreq               => loopback_fifo.rreq,
            q                   => loopback_fifo.rdata,
            rdempty             => loopback_fifo.rempty,
            rdfull              => loopback_fifo.rfull,
            rdusedw             => loopback_fifo.rused
        );


    -- Sample bridge
    U_fifo_writer : entity work.fifo_writer
      port map (
        clock               =>  rx_clock,
        reset               =>  rx_reset,
        enable              =>  rx_enable,

        usb_speed           =>  usb_speed,
        meta_en             =>  meta_en,
        timestamp           =>  rx_timestamp,

        fifo_full           =>  sample_fifo.wfull,
        fifo_usedw          =>  sample_fifo.wused,
        fifo_data           =>  sample_fifo.wdata,
        fifo_write          =>  sample_fifo.wreq,

        meta_fifo_full      =>  meta_fifo.wfull,
        meta_fifo_usedw     =>  meta_fifo.wused,
        meta_fifo_data      =>  meta_fifo.wdata,
        meta_fifo_write     =>  meta_fifo.wreq,

        in_i                =>  resize(rx_mux_i, 16),
        in_q                =>  resize(rx_mux_q, 16),
        in_valid            =>  rx_mux_valid,

        overflow_led        =>  rx_overflow_led,
        overflow_count      =>  open,
        overflow_duration   =>  x"ffff"
    );


    mimo_channel_sel_mux : process( rx_clock )
    begin
        if( rising_edge(rx_clock) ) then
            if( (mimo_channel_sel = '0') and (adc_0_enable = '1') ) then
                rx_sample_i     <= resize(signed(adc_0_i(11 downto 0)), rx_sample_i'length);
                rx_sample_q     <= resize(signed(adc_0_q(11 downto 0)), rx_sample_q'length);
                rx_sample_valid <= adc_0_valid;
            elsif( (mimo_channel_sel = '1') and (adc_1_enable = '1') ) then
                rx_sample_i     <= resize(signed(adc_1_i(11 downto 0)), rx_sample_i'length);
                rx_sample_q     <= resize(signed(adc_1_q(11 downto 0)), rx_sample_q'length);
                rx_sample_valid <= adc_1_valid;
            else
                rx_sample_i     <= to_signed(0, rx_sample_i'length);
                rx_sample_q     <= to_signed(0, rx_sample_q'length);
                rx_sample_valid <= '0';
            end if;
        end if;
    end process;


    loopback_fifo_control : process( rx_reset, loopback_fifo.rclock )
        constant WATERLINE          : natural := 16;
        variable already_strobed    : boolean := false;
    begin
        if( rx_reset = '1' ) then
            loopback_enabled   <= '0';
            loopback_fifo.rreq <= '0';
            loopback_i         <= (others => '0');
            loopback_q         <= (others => '0');
            loopback_valid     <= '0';
        elsif( rising_edge(loopback_fifo.rclock) ) then
            loopback_enabled   <= '0';
            loopback_fifo.rreq <= '0';
            loopback_i         <= loopback_i;
            loopback_q         <= loopback_q;
            loopback_valid     <= '0';

            -- Is loopback enabled?
            if( rx_mux_mode = RX_MUX_DIGITAL_LOOPBACK ) then
                loopback_enabled <= rx_enable;
            end if;

            -- Do the loopback
            if( loopback_enabled = '1' ) then
                loopback_i     <= resize(signed(loopback_fifo.rdata(15 downto 0)), loopback_i'length);
                loopback_q     <= resize(signed(loopback_fifo.rdata(31 downto 16)), loopback_q'length);
                loopback_valid <= loopback_fifo.rreq;
            end if;

            -- Read from the FIFO if req'd
            if( unsigned(loopback_fifo.rused) > WATERLINE ) then
                loopback_fifo.rreq <= loopback_enabled and (not loopback_fifo.rempty);
            end if;

            -- Handle situation where FIFO is empty but we tried to rreq from it
            if( (loopback_fifo.rreq = '1') and already_strobed ) then
                already_strobed := false;
                loopback_valid  <= '0';
            end if;

            -- Detect above condition
            already_strobed := ( (loopback_fifo.rreq = '1') and (loopback_fifo.rempty = '1') );
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
            rx_mux_i     <= (others =>'0');
            rx_mux_q     <= (others =>'0');
            rx_mux_valid <= '0';
            rx_gen_mode  <= '0';
        elsif( rising_edge(rx_clock) ) then
            case rx_mux_mode is
                when RX_MUX_NORMAL =>
                    rx_mux_i <= rx_sample_i;
                    rx_mux_q <= rx_sample_q;
                    if( trigger_signal_out_sync = '1' ) then
                        rx_mux_valid <= rx_sample_valid;
                    else
                        rx_mux_valid <= '0';
                    end if;
                when RX_MUX_12BIT_COUNTER | RX_MUX_32BIT_COUNTER =>
                    rx_mux_i     <= rx_gen_i;
                    rx_mux_q     <= rx_gen_q;
                    rx_mux_valid <= rx_gen_valid;
                    if( rx_mux_mode = RX_MUX_32BIT_COUNTER ) then
                        rx_gen_mode <= '1';
                    else
                        rx_gen_mode <= '0';
                    end if;
                when RX_MUX_ENTROPY =>
                    -- Not yet implemented
                    rx_mux_i     <= (others => '0');
                    rx_mux_q     <= (others => '0');
                    rx_mux_valid <= '0';
                when RX_MUX_DIGITAL_LOOPBACK =>
                    rx_mux_i     <= loopback_i;
                    rx_mux_q     <= loopback_q;
                    rx_mux_valid <= loopback_valid;
                when others =>
                    rx_mux_i     <= (others =>'0');
                    rx_mux_q     <= (others =>'0');
                    rx_mux_valid <= '0';
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
