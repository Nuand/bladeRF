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

entity fifo_reader is
  port (
    clock               :   in      std_logic;
    reset               :   in      std_logic;
    enable              :   in      std_logic;

    usb_speed           :   in      std_logic;
    meta_en             :   in      std_logic;
    timestamp           :   in      unsigned(63 downto 0);

    fifo_usedw          :   in      std_logic_vector(11 downto 0);
    fifo_read           :   buffer  std_logic := '0';
    fifo_empty          :   in      std_logic;
    fifo_data           :   in      std_logic_vector(31 downto 0);

    meta_fifo_usedw     :   in      std_logic_vector(2 downto 0);
    meta_fifo_read      :   buffer  std_logic := '0';
    meta_fifo_empty     :   in      std_logic;
    meta_fifo_data      :   in      std_logic_vector(127 downto 0);

    out_i               :   buffer  signed(15 downto 0) := (others => '0');
    out_q               :   buffer  signed(15 downto 0) := (others => '0');
    out_valid           :   buffer  std_logic := '0';

    underflow_led       :   buffer  std_logic;
    underflow_count     :   buffer  unsigned(63 downto 0);
    underflow_duration  :   in      unsigned(15 downto 0)
  );
end entity;

architecture simple of fifo_reader is

    constant DMA_BUF_SIZE_SS    : natural   := 1015;
    constant DMA_BUF_SIZE_HS    : natural   := 503;

    signal   dma_buf_size       : natural range DMA_BUF_SIZE_HS to DMA_BUF_SIZE_SS := DMA_BUF_SIZE_SS;
    signal   underflow_detected : std_logic := '0';

    type meta_state_t is (
        META_LOAD,
        META_WAIT,
        META_DOWNCOUNT
    );

    type meta_fsm_t is record
        state           : meta_state_t;
        dma_downcount   : natural range 0 to DMA_BUF_SIZE_SS;
        meta_read       : std_logic;
        meta_p_time     : unsigned(63 downto 0);
        meta_time_go    : std_logic;
    end record;

    constant META_FSM_RESET_VALUE : meta_fsm_t := (
        state           => META_LOAD,
        dma_downcount   => 0,
        meta_read       => '0',
        meta_p_time     => (others => '-'),
        meta_time_go    => '0'
    );

    signal meta_current : meta_fsm_t := META_FSM_RESET_VALUE;
    signal meta_future  : meta_fsm_t := META_FSM_RESET_VALUE;

    type fifo_state_t is (
        READ_SAMPLES,
        READ_THROTTLE
    );

    type fifo_fsm_t is record
        state           : fifo_state_t;
        downcount       : natural range 0 to 255;
        fifo_read       : std_logic;
        data_i          : signed(out_i'range);
        data_q          : signed(out_q'range);
        data_v          : std_logic;
    end record;

    constant FIFO_FSM_RESET_VALUE : fifo_fsm_t := (
        state           => READ_SAMPLES,
        downcount       => 0,
        fifo_read       => '0',
        data_i          => (others => '-'),
        data_q          => (others => '-'),
        data_v          => '0'
    );

    signal fifo_current : fifo_fsm_t := FIFO_FSM_RESET_VALUE;
    signal fifo_future  : fifo_fsm_t := FIFO_FSM_RESET_VALUE;

begin

    -- Determine the DMA buffer size based on USB speed
    calc_buf_size : process( clock, reset )
    begin
        if( reset = '1' ) then
            dma_buf_size <= DMA_BUF_SIZE_SS;
        elsif( rising_edge(clock) ) then
            if( usb_speed = '0' ) then
                dma_buf_size <= DMA_BUF_SIZE_SS;
            else
                dma_buf_size <= DMA_BUF_SIZE_HS;
            end if;
        end if;
    end process;


    -- ------------------------------------------------------------------------
    -- META FIFO FSM
    -- ------------------------------------------------------------------------

    -- Meta FIFO synchronous process
    meta_fsm_sync : process( clock, reset )
    begin
        if( reset = '1' ) then
            meta_current <= META_FSM_RESET_VALUE;
        elsif( rising_edge(clock) ) then
            meta_current <= meta_future;
        end if;
    end process;

    -- Meta FIFO combinatorial process
    meta_fsm_comb : process( all )
    begin

        meta_future <= meta_current;

        meta_future.meta_read <= '0';

        case meta_current.state is

            when META_LOAD =>

                meta_future.meta_p_time <= unsigned(meta_fifo_data(95 downto 32));

                if( meta_current.dma_downcount = 1 ) then
                    -- Finish up the previous meta block
                    meta_future.meta_time_go  <= '1';
                    meta_future.dma_downcount <= 0;
                else
                    meta_future.meta_time_go  <= '0';
                end if;

                if( meta_fifo_empty = '0' ) then
                    meta_future.meta_read <= '1';
                    meta_future.state     <= META_WAIT;
                end if;

            when META_WAIT =>

                meta_future.dma_downcount <= dma_buf_size;

                if( timestamp >= meta_current.meta_p_time ) then
                    meta_future.meta_time_go <= '1';
                    meta_future.state        <= META_DOWNCOUNT;
                end if;

            when META_DOWNCOUNT =>

                meta_future.meta_time_go  <= '1';
                meta_future.dma_downcount <= meta_current.dma_downcount - 1;

                if( meta_current.dma_downcount = 2 ) then
                    -- Look for 2 because of the 2 cycles passing
                    -- through META_LOAD and META_WAIT after this.
                    meta_future.state <= META_LOAD;
                end if;

            when others =>

                meta_future.state <= META_LOAD;

        end case;

        -- Abort?
        if( (enable = '0') or (meta_en = '0') ) then
            meta_future <= META_FSM_RESET_VALUE;
        end if;

        -- Output assignments
        meta_fifo_read <= meta_current.meta_read;

    end process;


    -- ------------------------------------------------------------------------
    -- SAMPLE FIFO FSM
    -- ------------------------------------------------------------------------

    -- Sample FIFO synchronous process
    fifo_fsm_sync : process( clock, reset )
    begin
        if( reset = '1' ) then
            fifo_current <= FIFO_FSM_RESET_VALUE;
        elsif( rising_edge(clock) ) then
            fifo_current <= fifo_future;
        end if;
    end process;

    -- Sample FIFO combinatorial process
    fifo_fsm_comb : process( all )
    begin

        fifo_future <= fifo_current;

        fifo_future.fifo_read <= '0';
        fifo_future.data_i    <= resize(signed(fifo_data(11 downto 0)) , fifo_future.data_i'length);
        fifo_future.data_q    <= resize(signed(fifo_data(27 downto 16)), fifo_future.data_q'length);
        fifo_future.data_v    <= '0';

        case fifo_current.state is

            when READ_SAMPLES =>

                fifo_future.downcount <= 0;

                if( fifo_empty = '0' and
                    (meta_en = '0' or (meta_en = '1' and meta_current.meta_time_go = '1')) ) then
                    fifo_future.fifo_read <= '1';
                    fifo_future.data_v    <= '1';
                    fifo_future.state     <= READ_THROTTLE;
                end if;

            when READ_THROTTLE =>

                if( fifo_current.downcount = 0 ) then
                    fifo_future.state     <= READ_SAMPLES;
                else
                    fifo_future.downcount <= fifo_current.downcount - 1;
                end if;

            when others =>

                fifo_future.state <= READ_SAMPLES;

        end case;

        -- Abort?
        if( enable = '0' ) then
            fifo_future.fifo_read <= '0';
            fifo_future.data_v    <= '0';
            fifo_future.state     <= FIFO_FSM_RESET_VALUE.state;
        end if;

        -- Output assignments
        fifo_read <= fifo_current.fifo_read;
        out_i     <= fifo_current.data_i;
        out_q     <= fifo_current.data_q;
        out_valid <= fifo_current.data_v;

    end process;


    -- ------------------------------------------------------------------------
    -- UNDERFLOW
    -- ------------------------------------------------------------------------

    -- Underflow detection
    detect_underflows : process( clock, reset )
    begin
        if( reset = '1' ) then
            underflow_detected <= '0';
        elsif( rising_edge( clock ) ) then
            underflow_detected <= '0';
            if( enable = '1' and fifo_empty = '1' and
                (meta_en = '0' or (meta_en = '1' and meta_current.meta_time_go = '1')) ) then
                underflow_detected <= '1';
            end if;
        end if;
    end process;

    -- Count the number of times we underflow, but only if they are discontinuous
    -- meaning we have an underflow condition, a non-underflow condition, then
    -- another underflow condition counts as 2 underflows, but an underflow condition
    -- followed by N underflow conditions counts as a single underflow condition.
    count_underflows : process( clock, reset )
        variable prev_underflow : std_logic := '0';
    begin
        if( reset = '1' ) then
            prev_underflow  := '0';
            underflow_count <= (others =>'0');
        elsif( rising_edge( clock ) ) then
            if( prev_underflow = '0' and underflow_detected = '1' ) then
                underflow_count <= underflow_count + 1;
            end if;
            prev_underflow := underflow_detected;
        end if;
    end process;

    -- Active high assertion for underflow_duration when the underflow
    -- condition has been detected.  The LED will stay asserted
    -- if multiple underflows have occurred
    blink_underflow_led : process( clock, reset )
        variable downcount : natural range 0 to 2**underflow_duration'length-1 := 0;
    begin
        if( reset = '1' ) then
            downcount     := 0;
            underflow_led <= '0';
        elsif( rising_edge(clock) ) then
            -- Default to not being asserted
            underflow_led <= '0';

            -- Countdown so we can see what happened
            if( downcount /= 0 ) then
                downcount     := downcount - 1;
                underflow_led <= '1';
            end if;

            -- Underflow occurred so light it up
            if( underflow_detected = '1' ) then
                downcount := to_integer(underflow_duration);
            end if;
        end if;
    end process;

end architecture;
