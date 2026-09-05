-- Does the GPIF state machine always return to IDLE when the host tears the
-- TX stream down mid-transaction while RX keeps running?
--
-- The machine has no timeout in any state. Every exit from META_WRITE,
-- SAMPLE_WRITE and SAMPLE_WRITE_IGNORE is a single equality test against a
-- downcount that saturates at -1, so a downcount that skips its exit value
-- parks the machine forever and the FX3 TX DMA channel never drains.
--
-- Runs standalone under GHDL: only nuand.fx3_gpif_p and set_clear_ff are
-- needed, no vendor PLL. The FX3 side is modelled by hand.
--
-- Generic SCENARIO:
--   0  steady TX+RX traffic, no host interference   (control, must pass)
--   1  meta_enable drops while a TX transaction is in flight
--   2  meta_enable rises while a TX transaction is in flight
--
-- Pass criterion: the machine reaches IDLE again within WATCHDOG cycles of
-- the disturbance. Failure is reported as a stuck state, which is the wedge.

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity fx3_gpif_wedge_tb is
    generic (
        SCENARIO : natural := 0
    );
end entity;

architecture arch of fx3_gpif_wedge_tb is

    constant HALF   : time    := 4 ns;      -- 125 MHz pclk
    constant USEDW  : natural := 14;

    signal pclk     : std_logic := '0';
    signal reset    : std_logic := '1';
    signal done     : boolean   := false;

    signal gpif_in  : std_logic_vector(31 downto 0) := (others => '0');
    signal gpif_out : std_logic_vector(31 downto 0);
    signal gpif_oe  : std_logic;
    signal ctl_in   : std_logic_vector(12 downto 0) := (others => '1');
    signal ctl_out  : std_logic_vector(12 downto 0);
    signal ctl_oe   : std_logic_vector(12 downto 0);

    signal tx_enable_o  : std_logic;
    signal rx_enable_o  : std_logic;
    signal meta_enable  : std_logic := '1';
    signal packet_enable: std_logic := '0';

    signal tx_fifo_write : std_logic;
    signal tx_fifo_data  : std_logic_vector(31 downto 0);
    signal tx_meta_write : std_logic;
    signal tx_meta_data  : std_logic_vector(31 downto 0);
    signal rx_fifo_read  : std_logic;
    signal rx_meta_read  : std_logic;

    signal tx_timestamp  : unsigned(63 downto 0) := (others => '0');
    signal freeze        : std_logic := '0';
    signal tx_dis        : std_logic := '0';
    signal tx_req_gone   : std_logic := '0';
    signal rx_req_gone   : std_logic := '0';
    signal rx_full       : std_logic := '0';

    -- 2048 words is enough for a GPIF burst but below rx_fifo_critical;
    -- rx_full pushes it over that line to flip the arbiter's priority.
    signal rx_usedw      : std_logic_vector(USEDW-1 downto 0);

    -- FX3 handshake, active low on the wire
    alias dma0_rx_ack   is ctl_out(0);
    alias dma3_tx_ack   is ctl_out(3);

    -- ctl_in bits driven by the FX3
    --   4 dma_rx_enable, 5 dma_tx_enable, 6 dma_idle,
    --   8 rx0 reqx, 10 tx2 reqx, 11 tx3 reqx, 12 rx1 reqx
begin

    clk : process
    begin
        while not done loop
            pclk <= '0'; wait for HALF;
            pclk <= '1'; wait for HALF;
        end loop;
        wait;
    end process;

    U_dut : entity work.fx3_gpif
        port map (
            pclk                => pclk,
            reset               => reset,
            usb_speed           => '0',            -- SuperSpeed

            gpif_in             => gpif_in,
            gpif_out            => gpif_out,
            gpif_oe             => gpif_oe,
            ctl_in              => ctl_in,
            ctl_out             => ctl_out,
            ctl_oe              => ctl_oe,

            tx_enable           => tx_enable_o,
            rx_enable           => rx_enable_o,
            meta_enable         => meta_enable,
            packet_enable       => packet_enable,

            tx_fifo_write       => tx_fifo_write,
            tx_fifo_full        => '0',
            tx_fifo_empty       => '1',
            tx_fifo_usedw       => std_logic_vector(to_unsigned(0, USEDW)),
            tx_fifo_data        => tx_fifo_data,

            tx_timestamp        => tx_timestamp,
            tx_meta_fifo_write  => tx_meta_write,
            tx_meta_fifo_full   => '0',
            tx_meta_fifo_empty  => '1',
            tx_meta_fifo_usedw  => std_logic_vector(to_unsigned(0, USEDW)),
            tx_meta_fifo_data   => tx_meta_data,

            rx_fifo_read        => rx_fifo_read,
            rx_fifo_full        => '0',
            rx_fifo_empty       => '0',
            rx_fifo_usedw       => rx_usedw,
            rx_fifo_data        => x"deadbeef",

            rx_meta_fifo_read   => rx_meta_read,
            rx_meta_fifo_full   => '0',
            rx_meta_fifo_empty  => '0',
            rx_meta_fifo_usedr  => std_logic_vector(to_unsigned(2**USEDW - 1, USEDW)),
            rx_meta_fifo_data   => x"00000010"
        );

    -- The FX3 always has a TX buffer ready and always wants RX data. dma_idle
    -- follows the ack lines: the FX3 drops idle while a transaction is acked.
    ctl_in(4)  <= '1';                      -- rx enabled
    ctl_in(5)  <= not tx_dis;               -- tx enabled
    ctl_in(6)  <= '0' when (freeze = '1' or dma0_rx_ack = '1' or
                           dma3_tx_ack = '1') else '1';
    rx_usedw   <= std_logic_vector(to_unsigned(7000, USEDW)) when rx_full = '1'
                  else std_logic_vector(to_unsigned(2048, USEDW));

    ctl_in(8)  <= rx_req_gone;              -- rx0 requesting (active low)
    ctl_in(10) <= '1';                      -- tx2 never used by the firmware
    ctl_in(11) <= tx_req_gone;              -- tx3 requesting (active low)
    ctl_in(12) <= '1';                      -- rx1 never used
    ctl_in(9)  <= '0';
    ctl_in(7)  <= '0';
    ctl_in(3 downto 0) <= (others => '0');

    -- A valid TX meta header: timestamp 0 means "now", which the machine
    -- accepts and routes to SAMPLE_WRITE.
    gpif_in <= (others => '0');

    -- Negative control: scenario 3 freezes the FX3 idle line, which is a real
    -- stop. If the watchdog below cannot see that, it cannot see the wedge
    -- either and every "alive" verdict is worthless.
    freeze <= '1' when SCENARIO = 3 else '0';

    stim : process
        variable tx_acks    : natural := 0;
        variable waited     : natural := 0;
        variable idle_seen  : natural := 0;
        variable stuck      : natural := 0;
        constant WATCHDOG   : natural := 4000;
    begin
        reset <= '1';
        wait for 20 * HALF;
        reset <= '0';

        -- Let transactions run, and count TX acks: without them the run only
        -- exercises RX and proves nothing about the TX path.
        for i in 1 to 600 loop
            wait until rising_edge(pclk);
            if dma3_tx_ack = '1' then
                tx_acks := tx_acks + 1;
            end if;
        end loop;
        assert tx_acks > 0 or SCENARIO = 3
            report "testbench broken: no TX transaction ever happened"
            severity failure;

        -- disturbance: the host reconfigures the TX stream, which flips
        -- meta_enable while the machine is inside a TX transaction
        if SCENARIO = 1 then
            wait until rising_edge(pclk) and dma3_tx_ack = '1';
            wait until rising_edge(pclk);
            wait until rising_edge(pclk);
            meta_enable <= '0';
        elsif SCENARIO = 2 then
            meta_enable <= '0';
            for i in 1 to 400 loop
                wait until rising_edge(pclk);
            end loop;
            wait until rising_edge(pclk) and dma3_tx_ack = '1';
            wait until rising_edge(pclk);
            wait until rising_edge(pclk);
            meta_enable <= '1';
        elsif SCENARIO = 4 then
            -- dma_tx_enable drops mid-transaction. This is what the host does
            -- through enable_module when it tears the TX stream down.
            wait until rising_edge(pclk) and dma3_tx_ack = '1';
            wait until rising_edge(pclk);
            wait until rising_edge(pclk);
            tx_dis <= '1';
            for i in 1 to 200 loop
                wait until rising_edge(pclk);
            end loop;
            tx_dis <= '0';
        elsif SCENARIO = 6 then
            -- The FX3 withdraws the TX buffer request mid-transaction. This is
            -- what a DmaChannelReset on the UtoP channel looks like from the
            -- FPGA side: the request line simply goes away while the FPGA is
            -- still counting down a burst it was acked for.
            wait until rising_edge(pclk) and dma3_tx_ack = '1';
            wait until rising_edge(pclk);
            tx_req_gone <= '1';
            for i in 1 to 200 loop
                wait until rising_edge(pclk);
            end loop;
            tx_req_gone <= '0';
        elsif SCENARIO = 7 then
            -- RX crosses the TX transaction: the host reading RX makes the
            -- FX3 raise and drop its RX request while a TX burst is being
            -- acked. This is the condition the wedge needs on hardware - TX
            -- alone never wedges, TX with RX being read does.
            -- bounded: a disturbance loop that waits on an ack forever would
            -- hang the bench instead of reporting the wedge it is looking for
            for k in 1 to 12 loop
                waited := 0;
                while dma3_tx_ack = '0' and waited < 4000 loop
                    wait until rising_edge(pclk);
                    waited := waited + 1;
                end loop;
                exit when waited >= 4000;
                rx_req_gone <= '1';
                for i in 1 to 3 loop
                    wait until rising_edge(pclk);
                end loop;
                rx_req_gone <= '0';
                waited := 0;
                while dma0_rx_ack = '0' and waited < 4000 loop
                    wait until rising_edge(pclk);
                    waited := waited + 1;
                end loop;
                exit when waited >= 4000;
                rx_req_gone <= '1';
                wait until rising_edge(pclk);
                rx_req_gone <= '0';
            end loop;
            report "scenario 7: disturbance loop ran " &
                   integer'image(waited) & " idle cycles at exit"
                   severity note;
        elsif SCENARIO = 8 then
            -- RX FIFO crosses the critical line mid-TX, which flips the
            -- arbiter's priority away from TX in the same cycle it is
            -- servicing one.
            for k in 1 to 12 loop
                wait until rising_edge(pclk) and dma3_tx_ack = '1';
                rx_full <= '1';
                for i in 1 to 5 loop
                    wait until rising_edge(pclk);
                end loop;
                rx_full <= '0';
            end loop;
        elsif SCENARIO = 5 then
            -- same, but the drop lands during the metadata phase
            wait until rising_edge(pclk) and dma3_tx_ack = '1';
            tx_dis <= '1';
            for i in 1 to 200 loop
                wait until rising_edge(pclk);
            end loop;
            tx_dis <= '0';
        end if;

        -- after the disturbance the machine must keep servicing the FX3.
        -- Progress is observable as continued acks; a wedged machine acks
        -- nothing ever again.
        idle_seen := 0;
        stuck     := 0;
        while idle_seen < 4 and stuck < WATCHDOG loop
            wait until rising_edge(pclk);
            stuck := stuck + 1;
            if dma3_tx_ack = '1' or dma0_rx_ack = '1' then
                idle_seen := idle_seen + 1;
                stuck     := 0;
            end if;
        end loop;

        if stuck >= WATCHDOG then
            report "WEDGED: no DMA ack for " & integer'image(WATCHDOG) &
                   " cycles after disturbance, scenario " &
                   integer'image(SCENARIO) severity failure;
        else
            report "alive: machine kept acking after scenario " &
                   integer'image(SCENARIO) severity note;
        end if;

        done <= true;
        wait;
    end process;

end architecture;
