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

entity fx3_gpif is
  port (
    -- FX3 control signals
    pclk                :   in  std_logic;
    reset               :   in  std_logic;

    -- USB Speed 0 = SS, 1 = HS
    usb_speed           :   in  std_logic;

    -- FX3 GPIF interface
    gpif_in             :   in  std_logic_vector(31 downto 0);
    gpif_out            :   out std_logic_vector(31 downto 0);
    gpif_oe             :   out std_logic;
    ctl_in              :   in  std_logic_vector(12 downto 0);
    ctl_out             :   out std_logic_vector(12 downto 0);
    ctl_oe              :   out std_logic_vector(12 downto 0);

    -- Enables
    tx_enable           :   out std_logic;
    rx_enable           :   out std_logic;
    meta_enable         :   in  std_logic;

    -- TX FIFO
    tx_fifo_write       :   out std_logic;
    tx_fifo_full        :   in  std_logic;
    tx_fifo_empty       :   in  std_logic;
    tx_fifo_usedw       :   in  std_logic_vector;
    tx_fifo_data        :   out std_logic_vector(31 downto 0);

    -- TX meta FIFO
    tx_timestamp        :   in  unsigned(63 downto 0);
    tx_meta_fifo_write  :   out std_logic;
    tx_meta_fifo_full   :   in  std_logic;
    tx_meta_fifo_empty  :   in  std_logic;
    tx_meta_fifo_usedw  :   in  std_logic_vector;
    tx_meta_fifo_data   :   out std_logic_vector(31 downto 0);

    -- RX FIFO
    rx_fifo_read        :   out std_logic;
    rx_fifo_full        :   in  std_logic;
    rx_fifo_empty       :   in  std_logic;
    rx_fifo_usedw       :   in  std_logic_vector;
    rx_fifo_data        :   in  std_logic_vector(31 downto 0);

    -- RX meta FIFO
    rx_meta_fifo_read   :   out std_logic;
    rx_meta_fifo_full   :   in  std_logic;
    rx_meta_fifo_empty  :   in  std_logic;
    rx_meta_fifo_usedr  :   in  std_logic_vector;
    rx_meta_fifo_data   :   in  std_logic_vector(31 downto 0)
  );
end entity;

architecture sample_shuffler of fx3_gpif is

    -- max(a, b) returns the greater of a or b
    function max( a : integer ; b : integer ) return integer is
    begin
        if (a > b) then
            return a;
        else
            return b;
        end if;
    end function;

    type state_t        is (IDLE, SETUP_RD, META_READ, SAMPLE_READ, SETUP_WR,
                            META_WRITE, SAMPLE_WRITE, SAMPLE_WRITE_IGNORE,
                            FINISHED);
    type gpif_mode_t    is (IDLE, RX, RX_META, RX_IGNORE,
                            TX, TX_META, TX_IGNORE);
    type dma_channel_t  is (RX0, RX1, TX2, TX3);

    -- Control mapping
    alias dma0_rx_ack   is ctl_out(0);
    alias dma1_rx_ack   is ctl_out(1);
    alias dma2_tx_ack   is ctl_out(2);
    alias dma3_tx_ack   is ctl_out(3);
    alias dma_rx_enable is ctl_in(4);
    alias dma_tx_enable is ctl_in(5);
    alias dma_idle      is ctl_in(6);
    alias dma0_rx_reqx  is ctl_in(8);
    alias dma1_rx_reqx  is ctl_in(12); -- due to 9 being connected to dclk
    alias dma2_tx_reqx  is ctl_in(10);
    alias dma3_tx_reqx  is ctl_in(11);

    -- Control OE (12 downto 0)
    constant CONTROL_OE : std_logic_vector := "0000000001111";

    -- State machine downcounter values
    constant ACK_DOWNCOUNT_READ     :   natural := 2;
    constant ACK_DOWNCOUNT_WRITE    :   natural := 4;
    constant META_DOWNCOUNT_RESET   :   natural := 4;
    constant FINI_DOWNCOUNT_RESET   :   natural := 10;

    -- GPIF buf sizes for USB high-speed and super-speed
    constant GPIF_BUF_SIZE_HS       :   natural := 256;
    constant GPIF_BUF_SIZE_SS       :   natural := 512;

    type dma_handshake_t is record
        rx0             :   std_logic;
        rx1             :   std_logic;
        tx2             :   std_logic;
        tx3             :   std_logic;
    end record;

    type fsm_t is record
        state           :   state_t;
        gpif_mode       :   gpif_mode_t;
        dma_idle        :   std_logic;
        rx_meta_en      :   std_logic;
        tx_meta_en      :   std_logic;
        rx_fifo_rd      :   std_logic;
        tx_fifo_wr      :   std_logic;
        rxm_fifo_rd     :   std_logic;
        txm_fifo_wr     :   std_logic;
        underrun_set    :   std_logic;
        underrun_clr    :   std_logic;
        ack_downcount   :   integer range  0 to max(ACK_DOWNCOUNT_READ, ACK_DOWNCOUNT_WRITE);
        dma_downcount   :   integer range -1 to GPIF_BUF_SIZE_SS;
        meta_downcount  :   integer range -1 to META_DOWNCOUNT_RESET;
        fini_downcount  :   integer range  0 to FINI_DOWNCOUNT_RESET;
        tx_ts_plus32    :   unsigned(63 downto 0);
        meta_buf        :   std_logic_vector(127 downto 0);
        dma_acks        :   dma_handshake_t;
        rx_current_dma  :   dma_channel_t;
        tx_current_dma  :   dma_channel_t;
    end record;

    constant FSM_RESET_VALUE : fsm_t := (
        state           =>  IDLE,
        gpif_mode       =>  IDLE,
        dma_idle        =>  '0',
        rx_meta_en      =>  '0',
        tx_meta_en      =>  '0',
        rx_fifo_rd      =>  '0',
        tx_fifo_wr      =>  '0',
        rxm_fifo_rd     =>  '0',
        txm_fifo_wr     =>  '0',
        underrun_set    =>  '0',
        underrun_clr    =>  '1',
        ack_downcount   =>  0,
        dma_downcount   =>  0,
        meta_downcount  =>  0,
        fini_downcount  =>  0,
        tx_ts_plus32    =>  (others => '0'),
        meta_buf        =>  (others => '0'),
        dma_acks        =>  (others => '0'),
        rx_current_dma  =>  RX0,
        tx_current_dma  =>  TX3
    );

    signal current, future      :   fsm_t := FSM_RESET_VALUE;

    signal can_rx, should_rx    :   boolean;
    signal can_tx, should_tx    :   boolean;
    signal rx_fifo_enough       :   boolean;
    signal tx_fifo_enough       :   boolean;
    signal rx_fifo_critical     :   boolean;

    signal underrun             :   std_logic;
    signal dma_req              :   dma_handshake_t;
    signal gpif_buf_size        :   natural range GPIF_BUF_SIZE_HS to GPIF_BUF_SIZE_SS := GPIF_BUF_SIZE_SS;

    attribute preserve                  :   boolean;
    attribute preserve  of can_rx       :   signal is true;
    attribute preserve  of can_tx       :   signal is true;
    attribute preserve  of should_rx    :   signal is true;
    attribute preserve  of should_tx    :   signal is true;

    attribute keep                      :   boolean;
    attribute keep      of can_rx       :   signal is true;
    attribute keep      of can_tx       :   signal is true;
    attribute keep      of should_rx    :   signal is true;
    attribute keep      of should_tx    :   signal is true;

    -- acknowledge(dma_channel) returns a dma_handshake_t with all bits 0
    -- except the bit corresponding to dma_channel
    function acknowledge( chan : dma_channel_t ) return dma_handshake_t is
        variable retval : dma_handshake_t := (others => '0');
    begin
        case (chan) is
            when RX0 => retval.rx0 := '1';
            when RX1 => retval.rx1 := '1';
            when TX2 => retval.tx2 := '1';
            when TX3 => retval.tx3 := '1';
        end case;

        return retval;
    end function;

begin
    -- DMA handshake signals (active-low, so inverting for clarity)
    dma_req.rx0 <= not dma0_rx_reqx;
    dma_req.rx1 <= not dma1_rx_reqx;
    dma_req.tx2 <= not dma2_tx_reqx;
    dma_req.tx3 <= not dma3_tx_reqx;

    -- Set FX3 control OEs and default the unused outputs
    ctl_oe                  <= CONTROL_OE;
    ctl_out(12 downto 4)    <= (others => '0');

    -- Set/clear flipflop for underrun indication
    U_underrun_ff : entity work.set_clear_ff
    port map (
        clock   => pclk,
        reset   => reset,
        set     => current.underrun_set,
        clear   => current.underrun_clr,
        q       => underrun
    );

    -- RX and TX enable signals for 
    enables : process(reset, pclk)
    begin
        if (reset = '1') then
            rx_enable <= '0';
            tx_enable <= '0';
        elsif (rising_edge(pclk)) then
            rx_enable <= dma_rx_enable;
            tx_enable <= dma_tx_enable;
        end if;
    end process enables;

    -- Transfer size for DMAs depends on USB speed
    calculate_conditionals : process(reset, pclk)
    begin
        if (reset = '1') then
            gpif_buf_size       <= GPIF_BUF_SIZE_SS;
        elsif (rising_edge(pclk)) then
            if (usb_speed = '0') then
                gpif_buf_size   <= GPIF_BUF_SIZE_SS;
            else
                gpif_buf_size   <= GPIF_BUF_SIZE_HS;
            end if;
        end if;
    end process calculate_conditionals;

    -- Manage FIFO capacity to avoid underruns/overruns
    calculate_fifo_waterlines : process(reset, pclk)
    begin
        if (reset = '1') then
            rx_fifo_enough      <= false;
            rx_fifo_critical    <= false;
            tx_fifo_enough      <= false;
        elsif (rising_edge(pclk)) then
            -- Do we have at least one block of data in RX buffer?
            rx_fifo_enough      <= (unsigned(rx_fifo_full&rx_fifo_usedw) >= gpif_buf_size);
            -- Can we not fit one more block of data in RX buffer?
            rx_fifo_critical    <= (unsigned(rx_fifo_usedw) >= ((2**(rx_fifo_usedw'length-1) - gpif_buf_size)));
            -- Do we have room for one more block of data in the TX buffer?
            tx_fifo_enough      <= (unsigned(tx_fifo_usedw) < ((2**(tx_fifo_usedw'length-1) - gpif_buf_size)));
        end if;
    end process calculate_fifo_waterlines;

    -- Handle gpif_oe and moving data to/from the gpif bus
    gpif_mux : process(reset, pclk)
    begin
        if (reset = '1') then
            gpif_oe             <= '1';
            gpif_out            <= (others => '0');
            tx_fifo_data        <= (others => '0');
            tx_meta_fifo_data   <= (others => '0');
        elsif (rising_edge(pclk)) then
            gpif_oe             <= '0';
            gpif_out            <= (others => '1');
            tx_fifo_data        <= (others => 'X');
            tx_meta_fifo_data   <= (others => 'X');

            case (current.gpif_mode) is
                when IDLE =>
                    gpif_oe         <= '0';

                when RX =>
                    gpif_oe         <= '1';
                    gpif_out        <= rx_fifo_data;

                when RX_META =>
                    gpif_oe         <= '1';

                    if (current.meta_downcount = 0) then
                        -- this overrites 16 LBSs in the flags field stored in fifo_writer
                        gpif_out    <= rx_meta_fifo_data(31 downto 16) & x"000" &
                            -- LSB nibble of the last word
                            "00" & (not underrun) & underrun;
                    else
                        gpif_out    <= rx_meta_fifo_data;
                    end if;

                when RX_IGNORE =>
                    gpif_oe         <= '1';
                    gpif_out        <= (others => '0');

                when TX =>
                    gpif_oe         <= '0';
                    tx_fifo_data    <= gpif_in;
                when TX_META =>
                    gpif_oe         <= '0';
                    tx_meta_fifo_data <= current.meta_buf(127 downto 96);

                when TX_IGNORE =>
                    gpif_oe         <= '0';

            end case;
        end if;
    end process gpif_mux;

    -- Arbitrate RX and TX requests
    -- In the event the FX3 simultaneously requests RX and TX, we generally
    -- prioritize the TX first; this helps avoid discontinuities on the TX
    -- signal. However, if RX is about to overflow, we'll choose that.
    can_and_should : process(pclk, reset)
    begin
        if (reset = '1') then
            can_rx      <= false;
            can_tx      <= false;
            should_rx   <= false;
            should_tx   <= false;
        elsif (rising_edge(pclk)) then
            can_rx      <= dma_rx_enable = '1' and rx_fifo_enough and (dma_req.rx0 = '1' or dma_req.rx1 = '1');
            can_tx      <= dma_tx_enable = '1' and tx_fifo_enough and (dma_req.tx2 = '1' or dma_req.tx3 = '1');
            should_rx   <= can_rx and (not can_tx or rx_fifo_critical);
            should_tx   <= can_tx and not rx_fifo_critical;
        end if;
    end process;

    -- Synchronous process for FSM
    fsm_sync_proc : process(reset, pclk) is
    begin
        if (reset = '1') then
            current <= FSM_RESET_VALUE;
        elsif (rising_edge(pclk)) then
            current <= future;
        end if;
    end process fsm_sync_proc;

    -- Combinatorial process for FSM
    fsm_comb_proc : process(all) is
        variable next_state : state_t := IDLE;
    begin
        -- Maintain current state unless otherwise specified
        future              <= current;

        -- Prevents inferred latch by making sure this variable always gets an
        -- assignment. (It is used as a temporary variable during SETUP_RD.)
        next_state          := IDLE;

        -- Deassert fifo reads/writes
        future.rx_fifo_rd   <= '0';
        future.tx_fifo_wr   <= '0';
        future.rxm_fifo_rd  <= '0';
        future.txm_fifo_wr  <= '0';

        -- Deassert underrun set/clear
        future.underrun_set <= '0';
        future.underrun_clr <= '0';

        -- Deassert DMA acknowledgements
        future.dma_acks     <= (others => '0');

        -- Register incoming signals
        future.dma_idle     <= dma_idle;
        future.tx_ts_plus32 <= tx_timestamp + 32;
        future.rx_meta_en   <= meta_enable;
        future.tx_meta_en   <= meta_enable;

        case (current.state) is
            -- =================================================================
            -- Idle State
            -- =================================================================
            when IDLE =>
                -- Set up initial conditions
                future.gpif_mode        <= IDLE;
                future.meta_buf         <= (others => '0');
                future.meta_downcount   <= META_DOWNCOUNT_RESET;
                future.fini_downcount   <= FINI_DOWNCOUNT_RESET;

                if (current.dma_idle = '1') then
                    if (should_rx and ( (rx_meta_fifo_empty = '0' and current.rx_meta_en = '1')
                                        or (current.rx_meta_en = '0') ) ) then
                        -- There is an RX to perform (sending data to FX3).
                        future.ack_downcount    <= ACK_DOWNCOUNT_READ;
                        future.dma_downcount    <= gpif_buf_size-1;
                        future.rx_current_dma   <= RX0;
                        future.state            <= SETUP_RD;

                    elsif (should_tx) then
                        -- There is a TX to perform (getting data from FX3).
                        future.ack_downcount    <= ACK_DOWNCOUNT_WRITE;
                        future.dma_downcount    <= gpif_buf_size-1;
                        future.tx_current_dma   <= TX3;
                        future.state            <= SETUP_WR;
                    end if;
                end if;

            -- =================================================================
            -- Read Handling (moving data to FX3)
            -- =================================================================
            when SETUP_RD =>
                -- SETUP_RD starts moving data from the FIFO to GPIF, while
                -- acknowledging the DMA request for ACK_DOWNCOUNT_READ
                -- clocks.

                -- GPIF, FIFO, next state depend on rx_meta_en
                if (current.rx_meta_en = '0') then
                    future.gpif_mode    <= RX;
                    future.rx_fifo_rd   <= '1';
                    next_state          := SAMPLE_READ;
                else
                    future.gpif_mode    <= RX_META;
                    future.rxm_fifo_rd  <= '1';
                    next_state          := META_READ;
                end if;

                -- After the first iteration (which gives time for the FIFO
                -- to produce valid data), assert the proper ack.
                if (current.ack_downcount /= ACK_DOWNCOUNT_READ) then
                    future.dma_acks         <= acknowledge(current.rx_current_dma);
                end if;

                -- Remain in this state until we're done acking
                if (current.ack_downcount = 0) then
                    future.state        <= next_state;
                else
                    future.state        <= current.state;
                end if;

                future.ack_downcount    <= max(current.ack_downcount-1, 0);
                future.dma_downcount    <= max(current.dma_downcount-1, -1);
                future.meta_downcount   <= max(current.meta_downcount-1, -1);

            when META_READ =>
                -- Service the metadata FIFO.
                future.gpif_mode        <= RX_META;
                future.rxm_fifo_rd      <= '1';

                -- After the meta is done, move on to the sample FIFO.
                if (current.meta_downcount = 1) then
                    future.state        <= SAMPLE_READ;
                end if;

                future.dma_downcount    <= max(current.dma_downcount-1, -1);
                future.meta_downcount   <= max(current.meta_downcount-1, -1);

            when SAMPLE_READ =>
                -- Service the sample FIFO.
                future.gpif_mode        <= RX;
                future.rx_fifo_rd       <= '1';

                -- Clear the underrun indicator.  This is set in the event
                -- of a TX underrun condition, and is sent back to the host
                -- as part of RX metadata.
                future.underrun_clr     <= '1';

                -- Once the DMA countdown is done, conclude this transaction
                if (current.dma_downcount = 0) then
                    future.state        <= FINISHED;
                end if;

                future.dma_downcount    <= max(current.dma_downcount-1, -1);

            -- =================================================================
            -- Write Handling (moving data from FX3)
            -- =================================================================
            when SETUP_WR =>
                -- SETUP_WR acknowledges the DMA request, and waits for
                -- ACK_DOWNCOUNT_WRITE clocks before moving onto servicing
                -- the FIFOs.

                -- Acknowledge DMA request
                future.dma_acks         <= acknowledge(current.tx_current_dma);

                -- Set GPIF mode (but not on first iteration)
                if (current.ack_downcount /= ACK_DOWNCOUNT_WRITE) then
                    future.gpif_mode    <= TX_IGNORE;
                end if;

                -- When we're done in this state, check tx_meta_en to decide
                -- where to go from here
                if (current.ack_downcount = 0) then
                    if (current.tx_meta_en = '0') then
                        future.state        <= SAMPLE_WRITE;
                        future.gpif_mode    <= TX;
                    else
                        future.state        <= META_WRITE;
                        future.gpif_mode    <= TX_META;
                    end if;
                end if;

                future.ack_downcount    <= max(current.ack_downcount-1, 0);

            when META_WRITE =>
                -- Fills meta_buf from GPIF
                future.gpif_mode        <= TX_IGNORE;
                future.meta_buf(127 downto 0) <= current.meta_buf(95 downto 0) & gpif_in(31 downto 0);

                future.dma_downcount    <= max(current.dma_downcount-1, -1);
                future.meta_downcount   <= max(current.meta_downcount-1, -1);

                -- Check meta_buf validity and determine next action
                if (current.meta_downcount = 1) then
                    if (unsigned(current.meta_buf(63 downto 0)) = 0 or
                        unsigned(current.meta_buf(31 downto 0) & current.meta_buf(63 downto 32)) > current.tx_ts_plus32)
                    then
                        future.meta_downcount <= META_DOWNCOUNT_RESET;
                        future.state    <= SAMPLE_WRITE;
                    else
                        future.state    <= SAMPLE_WRITE_IGNORE;
                    end if;
                end if;

            when SAMPLE_WRITE =>
                -- Move data from GPIF to the TX sample FIFO
                future.tx_fifo_wr       <= '1';

                -- Set target FIFO depending on tx_meta_en
                if (current.tx_meta_en = '0') then
                    future.gpif_mode    <= TX;
                else
                    future.gpif_mode    <= TX_META;
                end if;

                -- If meta_downcount is nonzero, flush meta_buf into meta FIFO
                if (current.meta_downcount > 0) then
                    future.txm_fifo_wr  <= current.tx_meta_en;
                    future.meta_buf(127 downto 0) <= current.meta_buf(95 downto 0) & x"00000000";
                end if;

                -- Determine when we are finished with the DMA transaction
                if (current.dma_downcount = 0) then
                    future.state        <= FINISHED;
                end if;

                future.dma_downcount    <= max(current.dma_downcount-1, -1);
                future.meta_downcount   <= max(current.meta_downcount-1, -1);

            when SAMPLE_WRITE_IGNORE =>
                -- This state is used to assert that an error situation
                -- occurred, and to clean out the sample buffer.  The underrun
                -- will be passed to the host in META_READ.
                future.gpif_mode        <= TX_IGNORE;
                future.underrun_set     <= '1';

                if (current.dma_downcount = 0) then
                    future.state        <= FINISHED;
                end if;

                future.dma_downcount    <= max(current.dma_downcount-1, -1);

            -- =================================================================
            -- End state
            -- =================================================================
            when FINISHED =>
                -- This provides a pause between adjacent transactions.
                future.gpif_mode        <= IDLE;
                future.fini_downcount   <= max(current.fini_downcount-1, 0);

                if (current.fini_downcount = 0) then
                    future.state        <= IDLE;
                end if;

        end case;
    end process fsm_comb_proc;

    -- Output process for FSM
    fsm_output_proc : process(current) is
    begin
        -- DMA acknowledgements
        dma0_rx_ack             <= current.dma_acks.rx0;
        dma1_rx_ack             <= current.dma_acks.rx1;
        dma2_tx_ack             <= current.dma_acks.tx2;
        dma3_tx_ack             <= current.dma_acks.tx3;

        -- FIFO control
        rx_fifo_read            <= current.rx_fifo_rd;
        tx_fifo_write           <= current.tx_fifo_wr;
        tx_meta_fifo_write      <= current.txm_fifo_wr;
        rx_meta_fifo_read       <= current.rxm_fifo_rd;
    end process fsm_output_proc;

end architecture sample_shuffler;
