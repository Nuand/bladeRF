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

library rtl_work;

library altera_lnsim;
    use altera_lnsim.altera_pll;

library work;
    use work.bladerf_p.all;

entity fx3_gpif_tb is
end entity;

architecture arch of fx3_gpif_tb is

    -- bladerf-hosted uses ad9361.clock (125 MHz) for rx_clock and tx_clock
    constant SYSCLK_HALF_PERIOD     : time  := 1 sec * (1.0/125.0e6/2.0);

    -- For reasons unknown, simulation of this system in ModelSim requires that
    -- wrreq and data be asserted shortly before the rising clock edge. Sigh...
    constant FIFO_WORKAROUND        : time  := 1 ps;

    type fx3_control_t is record
        usb_speed   :   std_logic;
        tx_enable   :   std_logic;
        rx_enable   :   std_logic;
        meta_enable :   std_logic;
    end record;

    type fx3_gpif_t is record
        gpif_in     :   std_logic_vector(31 downto 0);
        gpif_out    :   std_logic_vector(31 downto 0);
        gpif_oe     :   std_logic;
        ctl_in      :   std_logic_vector(12 downto 0);
        ctl_out     :   std_logic_vector(12 downto 0);
        ctl_oe      :   std_logic_vector(12 downto 0);
    end record;

    signal sys_rst                  : std_logic := '1';
    signal done                     : boolean := false;

    signal fx3_pclk                 : std_logic := '1';
    signal fx3_pclk_pll             : std_logic := '1';
    signal sys_clk                  : std_logic := '1';

    signal pll_reset                : std_logic := '0';
    signal pll_locked               : std_logic := '0';
    signal fx3_control              : fx3_control_t;
    signal fx3_gpif                 : fx3_gpif_t;
    signal fx3_gpif_i               : std_logic_vector(31 downto 0);
    signal fx3_ctl_i                : std_logic_vector(12 downto 0);

    signal loopback_i               : signed(15 downto 0) := (others =>'0');
    signal loopback_q               : signed(15 downto 0) := (others =>'0');
    signal loopback_valid           : std_logic           := '0';
    signal loopback_enabled         : std_logic           := '0';

    signal rx_sample_fifo           : fifo_t              := FIFO_T_DEFAULT;
    signal tx_sample_fifo           : fifo_t              := FIFO_T_DEFAULT;
    signal loopback_fifo            : fifo_t              := FIFO_T_DEFAULT;
    signal rx_meta_fifo             : meta_fifo_rx_t      := META_FIFO_RX_T_DEFAULT;
    signal tx_meta_fifo             : meta_fifo_tx_t      := META_FIFO_TX_T_DEFAULT;

    signal rx_mux_i                 : signed(15 downto 0);
    signal rx_mux_q                 : signed(15 downto 0);
    signal rx_mux_valid             : std_logic;

    signal tx_timestamp             : unsigned(63 downto 0);
    signal rx_timestamp             : unsigned(63 downto 0);



begin

    -- ========================================================================
    -- Brief summary of data flow
    -- ========================================================================
    --
    -- Simulated samples flow from fx3_model's TX side to its RX side. The
    -- validity of the data is checked there.
    --
    -- entity       U_fx3_model              FX3 model, TX process
    --  signal      fx3_gpif_i               fx3_gpif.gpif_oe is '0'
    --   process    register_gpif
    --  signal      fx3_gpif.gpif_in         Demuxed GPIF interface (FX3 -> FPGA)
    -- entity       U_fx3_gpif               GPIF implementation, write side
    --  signal      tx_sample_fifo.w*        TX fifo, write side
    -- entity       U_tx_sample_fifo
    --  signal      tx_sample_fifo.r*        TX fifo, read side
    --   process    tx_mux
    --  signal      loopback_fifo.w*         Loopback fifo, write side
    -- entity       U_rx_loopback_fifo
    --  signal      loopback_fifo.r*         Loopback fifo, read side
    --   process    loopback_fifo_control
    --  signal      loopback_{i,q,valid}
    --   process    rx_mux
    --  signal      rx_mux_{i,q,valid}
    -- entity       U_fifo_writer
    --  signal      rx_sample_fifo.w*        RX fifo, write side
    -- entity       U_rx_sample_fifo
    --  signal      rx_sample_fifo.r*        RX fifo, read side
    -- entity       U_fx3_gpif               GPIF implementation, read side
    --  signal      fx3_gpif.gpif_out        Demuxed GPIF interface (FPGA -> FX3)
    --   process    register_gpif
    --  signal      fx3_gpif_i               fx3_gpif.gpif_oe is '1'
    -- entity       U_fx3_model              FX3 model, RX process


    -- ========================================================================
    -- Instantiations
    -- ========================================================================

    -- Unit under test
    -- Clock domain: fx3_pclk_pll
    U_fx3_gpif : entity work.fx3_gpif
        port map (
            pclk                => fx3_pclk_pll,
            reset               => sys_rst,
            usb_speed           => fx3_control.usb_speed,
            gpif_in             => fx3_gpif.gpif_in,
            gpif_out            => fx3_gpif.gpif_out,
            gpif_oe             => fx3_gpif.gpif_oe,
            ctl_in              => fx3_gpif.ctl_in,
            ctl_out             => fx3_gpif.ctl_out,
            ctl_oe              => fx3_gpif.ctl_oe,
            tx_enable           => fx3_control.tx_enable,
            rx_enable           => fx3_control.rx_enable,
            meta_enable         => fx3_control.meta_enable,
            tx_fifo_write       => tx_sample_fifo.wreq,
            tx_fifo_full        => tx_sample_fifo.wfull,
            tx_fifo_empty       => tx_sample_fifo.wempty,
            tx_fifo_usedw       => tx_sample_fifo.wused,
            tx_fifo_data        => tx_sample_fifo.wdata,
            tx_timestamp        => tx_timestamp,
            tx_meta_fifo_write  => tx_meta_fifo.wreq,
            tx_meta_fifo_full   => tx_meta_fifo.wfull,
            tx_meta_fifo_empty  => tx_meta_fifo.wempty,
            tx_meta_fifo_usedw  => tx_meta_fifo.wused,
            tx_meta_fifo_data   => tx_meta_fifo.wdata,
            rx_fifo_read        => rx_sample_fifo.rreq,
            rx_fifo_full        => rx_sample_fifo.rfull,
            rx_fifo_empty       => rx_sample_fifo.rempty,
            rx_fifo_usedw       => rx_sample_fifo.rused,
            rx_fifo_data        => rx_sample_fifo.rdata,
            rx_meta_fifo_read   => rx_meta_fifo.rreq,
            rx_meta_fifo_full   => rx_meta_fifo.rfull,
            rx_meta_fifo_empty  => rx_meta_fifo.rempty,
            rx_meta_fifo_usedr  => rx_meta_fifo.rused,
            rx_meta_fifo_data   => rx_meta_fifo.rdata
        );

    -- Discrete control signals for fx3_gpif
    fx3_control.usb_speed   <= '0';
    fx3_control.meta_enable <= '0';

    tx_timestamp  <= (others => '0');
    rx_timestamp  <= (others => '0');

    -- Model of FX3's GPIF interface
    -- Clock domain: fx3_pclk (source)
    U_fx3_model : entity work.fx3_model(dma)
        port map (
            fx3_pclk            => fx3_pclk,
            fx3_gpif            => fx3_gpif_i,
            fx3_ctl             => fx3_ctl_i,
            fx3_uart_rxd        => '0',
            fx3_uart_txd        => open,
            fx3_uart_cts        => open,
            fx3_rx_en           => '1',
            fx3_rx_meta_en      => '0',
            fx3_tx_en           => '1',
            fx3_tx_meta_en      => '0',
            done                => done
        );

    -- Generate phase-shifted PLL clock
    U_fx3_pll : entity rtl_work.fx3_pll
        port map (
            refclk   =>  fx3_pclk,
            rst      =>  pll_reset,
            outclk_0 =>  fx3_pclk_pll,
            locked   =>  pll_locked
        );

    -- Reset handler
    sys_rst <= '0' after 100 ns;

    -- Generate system clock
    sys_clk <= not sys_clk after SYSCLK_HALF_PERIOD when not done else '0';

    -- TX fifos
    -- Data from fx3_model to fx3_gpif
    -- Clock domain: fx3_pclk_pll in, sys_clk out
    tx_sample_fifo.aclr   <= sys_rst;
    tx_sample_fifo.wclock <= fx3_pclk_pll after FIFO_WORKAROUND; -- simulation workaround
    tx_sample_fifo.rclock <= sys_clk;
    U_tx_sample_fifo : entity work.tx_fifo
        port map (
            aclr                => tx_sample_fifo.aclr,

            wrclk               => tx_sample_fifo.wclock,
            wrreq               => tx_sample_fifo.wreq,
            data                => tx_sample_fifo.wdata,
            wrempty             => tx_sample_fifo.wempty,
            wrfull              => tx_sample_fifo.wfull,
            wrusedw             => tx_sample_fifo.wused,

            rdclk               => tx_sample_fifo.rclock,
            rdreq               => tx_sample_fifo.rreq,
            q                   => tx_sample_fifo.rdata,
            rdempty             => tx_sample_fifo.rempty,
            rdfull              => tx_sample_fifo.rfull,
            rdusedw             => tx_sample_fifo.rused
        );

    tx_meta_fifo.aclr   <= sys_rst;
    tx_meta_fifo.wclock <= fx3_pclk_pll after FIFO_WORKAROUND; -- simulation workaround
    tx_meta_fifo.rclock <= sys_clk;
    U_tx_meta_fifo : entity work.tx_meta_fifo
        port map (
            aclr                => tx_meta_fifo.aclr,

            wrclk               => tx_meta_fifo.wclock,
            wrreq               => tx_meta_fifo.wreq,
            data                => tx_meta_fifo.wdata,
            wrempty             => tx_meta_fifo.wempty,
            wrfull              => tx_meta_fifo.wfull,
            wrusedw             => tx_meta_fifo.wused,

            rdclk               => tx_meta_fifo.rclock,
            rdreq               => tx_meta_fifo.rreq,
            q                   => tx_meta_fifo.rdata,
            rdempty             => tx_meta_fifo.rempty,
            rdfull              => tx_meta_fifo.rfull,
            rdusedw             => tx_meta_fifo.rused
        );

    -- Loopback FIFO
    -- Data from TX (fx3_model->fx3_gpif) to RX (fx3_gpif->fx3_model)
    -- Clock domain: sys_clk
    loopback_fifo.aclr   <= sys_rst;
    loopback_fifo.wclock <= sys_clk after FIFO_WORKAROUND; -- simulation workaround
    loopback_fifo.rclock <= sys_clk;
    U_rx_loopback_fifo : entity work.rx_fifo
        port map (
            aclr                => loopback_fifo.aclr,

            wrclk               => loopback_fifo.wclock,
            wrreq               => loopback_fifo.wreq,
            data                => loopback_fifo.wdata,
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
    -- Clock domain: sys_clk
    U_fifo_writer : entity work.fifo_writer
        port map (
            clock               =>  sys_clk,
            reset               =>  sys_rst,
            enable              =>  fx3_control.rx_enable,

            usb_speed           =>  fx3_control.usb_speed,
            meta_en             =>  fx3_control.meta_enable,
            timestamp           =>  rx_timestamp,

            fifo_full           =>  rx_sample_fifo.wfull,
            fifo_usedw          =>  rx_sample_fifo.wused,
            fifo_data           =>  rx_sample_fifo.wdata,
            fifo_write          =>  rx_sample_fifo.wreq,

            meta_fifo_full      =>  rx_meta_fifo.wfull,
            meta_fifo_usedw     =>  rx_meta_fifo.wused,
            meta_fifo_data      =>  rx_meta_fifo.wdata,
            meta_fifo_write     =>  rx_meta_fifo.wreq,

            in_i                =>  resize(rx_mux_i, 16),
            in_q                =>  resize(rx_mux_q, 16),
            in_valid            =>  rx_mux_valid,

            overflow_led        =>  open,
            overflow_count      =>  open,
            overflow_duration   =>  x"ffff"
        );

    -- RX FIFOs
    -- Data from fx3_gpif to fx3_model
    -- Clock domain: sys_clk in, fx3_pclk_pll out
    rx_sample_fifo.aclr   <= sys_rst;
    rx_sample_fifo.wclock <= sys_clk after FIFO_WORKAROUND; -- simulation workaround
    rx_sample_fifo.rclock <= fx3_pclk_pll;
    U_rx_sample_fifo : entity work.rx_fifo
        port map (
            aclr                => rx_sample_fifo.aclr,

            wrclk               => rx_sample_fifo.wclock,
            wrreq               => rx_sample_fifo.wreq,
            data                => rx_sample_fifo.wdata,
            wrempty             => rx_sample_fifo.wempty,
            wrfull              => rx_sample_fifo.wfull,
            wrusedw             => rx_sample_fifo.wused,

            rdclk               => rx_sample_fifo.rclock,
            rdreq               => rx_sample_fifo.rreq,
            q                   => rx_sample_fifo.rdata,
            rdempty             => rx_sample_fifo.rempty,
            rdfull              => rx_sample_fifo.rfull,
            rdusedw             => rx_sample_fifo.rused
        );

    rx_meta_fifo.aclr   <= sys_rst;
    rx_meta_fifo.wclock <= sys_clk after FIFO_WORKAROUND; -- simulation workaround
    rx_meta_fifo.rclock <= fx3_pclk_pll;
    U_rx_meta_fifo : entity work.rx_meta_fifo
        port map (
            aclr                => rx_meta_fifo.aclr,

            wrclk               => rx_meta_fifo.wclock,
            wrreq               => rx_meta_fifo.wreq,
            data                => rx_meta_fifo.wdata,
            wrempty             => rx_meta_fifo.wempty,
            wrfull              => rx_meta_fifo.wfull,
            wrusedw             => rx_meta_fifo.wused,

            rdclk               => fx3_pclk_pll,
            rdreq               => rx_meta_fifo.rreq,
            q                   => rx_meta_fifo.rdata,
            rdempty             => rx_meta_fifo.rempty,
            rdfull              => rx_meta_fifo.rfull,
            rdusedw             => rx_meta_fifo.rused
        );


    -- ========================================================================
    -- Processes
    -- ========================================================================

    -- FX3 GPIF bidirectional signal control
    -- Adapted from same process in bladerf-hosted.vhd
    register_gpif : process(sys_rst, fx3_pclk_pll)
    begin
        if( sys_rst = '1' ) then
            fx3_gpif_i          <= (others => 'Z');
            fx3_gpif.gpif_in    <= (others => 'Z');
        elsif( rising_edge(fx3_pclk_pll) ) then
            fx3_gpif.gpif_in    <= fx3_gpif_i;

            if( fx3_gpif.gpif_oe = '1' ) then
                fx3_gpif_i      <= fx3_gpif.gpif_out;
            else
                fx3_gpif_i      <= (others =>'Z');
            end if;
        end if;
    end process;

    -- FX3 CTL bidirectional signals
    -- Adapted from same generator in bladerf-hosted.vhd
    generate_ctl : for i in fx3_ctl_i'range generate
        fx3_ctl_i(i) <= fx3_gpif.ctl_out(i) when fx3_gpif.ctl_oe(i) = '1' else 'Z';
    end generate;

    fx3_gpif.ctl_in <= fx3_ctl_i;

    -- Controller for loopback FIFO
    -- Stripped-down version of same process from rx.vhd
    loopback_fifo_control : process( sys_rst, loopback_fifo.rclock )
    begin
        if( sys_rst = '1' ) then
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
            loopback_enabled <= fx3_control.rx_enable;

            -- Do the loopback
            loopback_i     <= resize(signed(loopback_fifo.rdata(15 downto 0)), loopback_i'length);
            loopback_q     <= resize(signed(loopback_fifo.rdata(31 downto 16)), loopback_q'length);
            loopback_valid <= loopback_fifo.rreq and not loopback_fifo.rempty;

            -- Read from the FIFO if req'd
            loopback_fifo.rreq <= loopback_enabled and (not loopback_fifo.rempty);
        end if;
    end process;

    -- RX multiplexer
    -- Stripped-down version of same process from rx.vhd
    rx_mux : process(sys_rst, sys_clk)
    begin
        if( sys_rst = '1' ) then
            rx_mux_i     <= (others =>'0');
            rx_mux_q     <= (others =>'0');
            rx_mux_valid <= '0';
        elsif( rising_edge(sys_clk) ) then
            rx_mux_i     <= loopback_i;
            rx_mux_q     <= loopback_q;
            rx_mux_valid <= loopback_valid;
        end if;
    end process;

    -- TX multiplexer
    -- Stripped-down version of mimo_channel_sel_mux from tx.vhd
    tx_mux : process( sys_clk )
    begin
        if( rising_edge(sys_clk) ) then
            tx_sample_fifo.rreq <= not tx_sample_fifo.rempty;

            loopback_fifo.wdata <= tx_sample_fifo.rdata;
            loopback_fifo.wreq  <= tx_sample_fifo.rreq and not tx_sample_fifo.rempty;
        end if;
    end process;


    -- ========================================================================
    -- Validation
    -- ========================================================================

    -- Check for metavalues
    -- We should never see metavalues written to the FIFOs
    check_fifo_write : process (fx3_pclk_pll) is
    begin
        if (rising_edge(fx3_pclk_pll)) then
            if (tx_sample_fifo.wreq = '1') then
                for i in tx_sample_fifo.wdata'range loop
                    assert tx_sample_fifo.wdata(i) = '0' or tx_sample_fifo.wdata(i) = '1'
                    severity failure;
                end loop;
            end if;
        end if;
    end process check_fifo_write;

    check_fifo_read : process (fx3_pclk_pll) is
    begin
        if (rising_edge(fx3_pclk_pll)) then
            if (rx_sample_fifo.rreq = '1') then
                for i in rx_sample_fifo.rdata'range loop
                    assert rx_sample_fifo.rdata(i) = '0' or rx_sample_fifo.rdata(i) = '1'
                    severity failure;
                end loop;
            end if;
        end if;
    end process check_fifo_read;

    -- Check for fullness on FIFOs
    assert (rx_sample_fifo.wfull = '0') report "rx_sample_fifo full (write)" severity warning;
    assert (tx_sample_fifo.wfull = '0') report "tx_sample_fifo full (write)" severity warning;
    assert (loopback_fifo.wfull = '0') report "loopback_fifo full (write)" severity warning;

end architecture;
