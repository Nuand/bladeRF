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

entity tx is
    port(
        tx_reset             : in    std_logic;
        tx_clock             : in    std_logic;
        tx_enable            : in    std_logic;

        meta_en              : in    std_logic := '0';
        timestamp_reset      : out   std_logic := '1';
        usb_speed            : in    std_logic;
        tx_timestamp         : in    unsigned(63 downto 0);
        mimo_channel_sel     : in    std_logic := '0'; -- temporary

        -- Triggering
        trigger_arm          : in    std_logic;
        trigger_fire         : in    std_logic;
        trigger_master       : in    std_logic;
        trigger_line         : inout std_logic; -- this is not good, should be in/out/oe

        -- Samples from host via FX3
        sample_fifo_wclock   : in    std_logic;
        sample_fifo_wreq     : in    std_logic;
        sample_fifo_wdata    : in    std_logic_vector(TX_FIFO_T_DEFAULT.wdata'range);
        sample_fifo_wempty   : out   std_logic;
        sample_fifo_wfull    : out   std_logic;
        sample_fifo_wused    : out   std_logic_vector(TX_FIFO_T_DEFAULT.wused'range);

        -- Metadata from host via FX3
        meta_fifo_wclock     : in    std_logic;
        meta_fifo_wreq       : in    std_logic;
        meta_fifo_wdata      : in    std_logic_vector(META_FIFO_TX_T_DEFAULT.wdata'range);
        meta_fifo_wempty     : out   std_logic;
        meta_fifo_wfull      : out   std_logic;
        meta_fifo_wused      : out   std_logic_vector(META_FIFO_TX_T_DEFAULT.wused'range);

        -- Digital Loopback Interface
        loopback_enabled     : in    std_logic;
        loopback_data        : out   std_logic_vector(RX_FIFO_T_DEFAULT.wdata'range);
        loopback_data_valid  : out   std_logic;

        -- RFFE Interface
        dac_0_enable         : in    std_logic := '0';
        dac_0_request        : in    std_logic := '0';
        dac_0_i              : out   std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(0).dac.i.data'range);
        dac_0_q              : out   std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(0).dac.q.data'range);
        dac_1_enable         : in    std_logic := '0';
        dac_1_request        : in    std_logic := '0';
        dac_1_i              : out   std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(1).dac.i.data'range);
        dac_1_q              : out   std_logic_vector(MIMO_2R2T_T_DEFAULT.ch(1).dac.q.data'range)
    );
end entity;

architecture arch of tx is

    signal sample_fifo                    : tx_fifo_t      := TX_FIFO_T_DEFAULT;
    signal meta_fifo                      : meta_fifo_tx_t := META_FIFO_TX_T_DEFAULT;

    signal trigger_arm_sync               : std_logic;
    signal trigger_line_sync              : std_logic;
    signal sample_fifo_rempty_untriggered : std_logic;

begin

    set_timestamp_reset : process(tx_clock, tx_reset)
    begin
        if( tx_reset = '1' ) then
            timestamp_reset <= '1';
        elsif( rising_edge(tx_clock) ) then
            if( meta_en = '1' ) then
                timestamp_reset <= '0';
            else
                timestamp_reset <= '1';
            end if;
        end if;
    end process;

    -- TX sample fifo
    sample_fifo.aclr   <= tx_reset;
    sample_fifo.rclock <= tx_clock;
    U_tx_sample_fifo : entity work.tx_fifo
        generic map (
            LPM_NUMWORDS        => 2**(sample_fifo.wused'length)
        ) port map (
            aclr                => sample_fifo.aclr,

            wrclk               => sample_fifo_wclock,
            wrreq               => sample_fifo_wreq,
            data                => sample_fifo_wdata,
            wrempty             => sample_fifo_wempty,
            wrfull              => sample_fifo_wfull,
            wrusedw             => sample_fifo_wused,

            rdclk               => sample_fifo.rclock,
            rdreq               => sample_fifo.rreq,
            q                   => sample_fifo.rdata,
            rdempty             => sample_fifo_rempty_untriggered,
            rdfull              => sample_fifo.rfull,
            rdusedw             => sample_fifo.rused
        );

    -- TX metadata fifo
    meta_fifo.aclr   <= tx_reset;
    meta_fifo.rclock <= tx_clock;
    U_tx_meta_fifo : entity work.tx_meta_fifo
        generic map (
            LPM_NUMWORDS        => 2**(meta_fifo.wused'length)
        ) port map (
            aclr                => meta_fifo.aclr,

            wrclk               => meta_fifo_wclock,
            wrreq               => meta_fifo_wreq,
            data                => meta_fifo_wdata,
            wrempty             => meta_fifo_wempty,
            wrfull              => meta_fifo_wfull,
            wrusedw             => meta_fifo_wused,

            rdclk               => meta_fifo.rclock,
            rdreq               => meta_fifo.rreq,
            q                   => meta_fifo.rdata,
            rdempty             => meta_fifo.rempty,
            rdfull              => meta_fifo.rfull,
            rdusedw             => meta_fifo.rused
        );

    mimo_channel_sel_mux : process( tx_clock )
        variable fifo_rreq : std_logic := '0';

        alias fifo_i : std_logic_vector(11 downto 0) is sample_fifo.rdata(11 downto 0);
        alias fifo_q : std_logic_vector(11 downto 0) is sample_fifo.rdata(27 downto 16);
    begin
        if( rising_edge(tx_clock) ) then

            sample_fifo.rreq    <= '0';

            if( sample_fifo.rempty = '0' ) then
                if( (mimo_channel_sel = '0') and (dac_0_enable = '1') ) then
                    sample_fifo.rreq <= dac_0_request;
                elsif( (mimo_channel_sel = '1') and (dac_1_enable = '1') ) then
                    sample_fifo.rreq <= dac_1_request;
                else
                    sample_fifo.rreq <= '0';
                end if;

                if( loopback_enabled = '1' ) then
                    sample_fifo.rreq    <= '1';
                else
                    sample_fifo.rreq    <= '0';
                end if;
            end if;

            if( mimo_channel_sel = '0' ) then
                dac_0_i <= fifo_i & "0000";
                dac_0_q <= fifo_q & "0000";
            else
                dac_1_i <= fifo_i & "0000";
                dac_1_q <= fifo_q & "0000";
            end if;

            loopback_data       <= sample_fifo.rdata;
            loopback_data_valid <= sample_fifo.rreq and not sample_fifo.rempty;

        end if;
    end process;

    --U_fifo_reader : entity work.fifo_reader
    --  port map (
    --    clock               =>  tx_clock,
    --    reset               =>  tx_reset,
    --    enable              =>  tx_enable,
    --
    --    usb_speed           =>  usb_speed_tx,
    --    meta_en             =>  meta_en,
    --    timestamp           =>  tx_timestamp,
    --
    --    fifo_empty          =>  sample_fifo.rempty,
    --    fifo_usedw          =>  sample_fifo.rused,
    --    fifo_data           =>  sample_fifo.rdata,
    --    fifo_read           =>  sample_fifo.rreq,
    --
    --    meta_fifo_empty     =>  meta_fifo.rempty,
    --    meta_fifo_usedw     =>  meta_fifo.rused,
    --    meta_fifo_data      =>  meta_fifo.rdata,
    --    meta_fifo_read      =>  meta_fifo.rreq,
    --
    --    out_i               =>  tx_sample_i,
    --    out_q               =>  tx_sample_q,
    --    out_valid           =>  tx_sample_valid,
    --
    --    underflow_led       =>  tx_underflow_led,
    --    underflow_count     =>  open,
    --    underflow_duration  =>  x"ffff"
    --  ) ;

    txtrig : entity work.trigger(async)
        generic map (
            DEFAULT_OUTPUT  => '1'
        )
        port map (
            armed           => trigger_arm_sync,               -- in  sl
            fired           => trigger_fire,                   -- in  sl
            master          => trigger_master,                 -- in  sl
            trigger_in      => trigger_line_sync,              -- in  sl
            trigger_out     => trigger_line,                   -- out sl
            signal_in       => sample_fifo_rempty_untriggered, -- in  sl
            signal_out      => sample_fifo.rempty              -- out sl
        );


    U_reset_sync_txtrig_arm : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL     =>  '0',
            OUTPUT_LEVEL    =>  '0'
        )
        port map (
            clock           =>  tx_clock,
            async           =>  trigger_arm,
            sync            =>  trigger_arm_sync
        );

    U_sync_txtrig_trigger : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        )
        port map (
            reset       =>  tx_reset,
            clock       =>  tx_clock,
            async       =>  trigger_line,
            sync        =>  trigger_line_sync
        );

end architecture;
