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
    use work.fifo_readwrite_p.all;

entity fifo_writer is
    generic (
        NUM_STREAMS           : natural := 1;
        FIFO_USEDW_WIDTH      : natural := 12;
        FIFO_DATA_WIDTH       : natural := 32;
        META_FIFO_USEDW_WIDTH : natural := 5;
        META_FIFO_DATA_WIDTH  : natural := 128
    );
    port (
        clock               :   in      std_logic;
        reset               :   in      std_logic;
        enable              :   in      std_logic;

        usb_speed           :   in      std_logic;
        meta_en             :   in      std_logic;
        timestamp           :   in      unsigned(63 downto 0);
        mini_exp            :   in      std_logic_vector(1 downto 0);

        in_sample_controls  :   in      sample_controls_t(0 to NUM_STREAMS-1) := (others => SAMPLE_CONTROL_DISABLE);
        in_samples          :   in      sample_streams_t(0 to NUM_STREAMS-1)  := (others => ZERO_SAMPLE);

        fifo_usedw          :   in      std_logic_vector(FIFO_USEDW_WIDTH-1 downto 0);
        fifo_clear          :   buffer  std_logic;
        fifo_write          :   buffer  std_logic := '0';
        fifo_full           :   in      std_logic;
        fifo_data           :   out     std_logic_vector(FIFO_DATA_WIDTH-1 downto 0) := (others => '0');

        meta_fifo_full      :   in     std_logic;
        meta_fifo_usedw     :   in     std_logic_vector(META_FIFO_USEDW_WIDTH-1 downto 0);
        meta_fifo_data      :   out    std_logic_vector(META_FIFO_DATA_WIDTH-1 downto 0) := (others => '0');
        meta_fifo_write     :   out    std_logic := '0';

        overflow_led        :   buffer  std_logic;
        overflow_count      :   buffer  unsigned(63 downto 0);
        overflow_duration   :   in      unsigned(15 downto 0)
    );
end entity;

architecture simple of fifo_writer is

    constant DMA_BUF_SIZE_SS   : natural   := 1015;
    constant DMA_BUF_SIZE_HS   : natural   := 503;

    signal dma_buf_size        : natural range DMA_BUF_SIZE_HS to DMA_BUF_SIZE_SS := DMA_BUF_SIZE_SS;

    signal fifo_enough         : boolean   := false;
    signal overflow_detected   : std_logic := '0';

    type meta_state_t is (
        IDLE,
        META_WRITE,
        META_DOWNCOUNT
    );

    type meta_fsm_t is record
        state           : meta_state_t;
        dma_downcount   : natural range 0 to DMA_BUF_SIZE_SS;
        meta_write      : std_logic;
        meta_data       : std_logic_vector(meta_fifo_data'range);
        meta_written    : std_logic;
    end record;

    constant META_FSM_RESET_VALUE : meta_fsm_t := (
        state           => IDLE,
        dma_downcount   => 0,
        meta_write      => '0',
        meta_data       => (others => '-'),
        meta_written    => '0'
    );

    signal meta_current : meta_fsm_t := META_FSM_RESET_VALUE;
    signal meta_future  : meta_fsm_t := META_FSM_RESET_VALUE;

    type fifo_state_t is (
        CLEAR,
        WRITE_SAMPLES,
        HOLDOFF
    );

    type ch_offsets_t is array( natural range <> ) of natural range fifo_data'low to fifo_data'high;

    type fifo_fsm_t is record
        state               : fifo_state_t;
        fifo_clear          : std_logic;
        fifo_write          : std_logic;
        fifo_data           : unsigned(fifo_data'range);
        samples_left        : natural range 0 to in_sample_controls'length;
    end record;

    constant FIFO_FSM_RESET_VALUE : fifo_fsm_t := (
        state               => CLEAR,
        fifo_clear          => '1',
        fifo_write          => '0',
        fifo_data           => (others => '-'),
        samples_left        => 0
    );

    signal fifo_current : fifo_fsm_t := FIFO_FSM_RESET_VALUE;
    signal fifo_future  : fifo_fsm_t := FIFO_FSM_RESET_VALUE;

    signal sync_mini_exp: std_logic_vector(1 downto 0);

begin

    -- Throw an error if port widths don't make sense
    assert (fifo_data'length >= (NUM_STREAMS*2*in_samples(in_samples'low).data_i'length) )
        report "fifo_data port width too narrow to support " & integer'image(NUM_STREAMS) & " MIMO streams."
        severity failure;

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

    -- Calculate whether there's enough room in the destination FIFO
    calc_fifo_free : process( clock, reset )
        constant FIFO_MAX    : natural := 2**fifo_usedw'length;
        variable fifo_used_v : unsigned(fifo_usedw'length downto 0) := (others => '0');
    begin
        if( reset = '1' ) then
            fifo_enough <= false;
            fifo_used_v := (others => '0');
        elsif( rising_edge(clock) ) then
            fifo_used_v := unsigned(fifo_full & fifo_usedw);
            if( fifo_full = '0' and ((FIFO_MAX - fifo_used_v) > dma_buf_size) and
                 ( ( meta_en = '1' and meta_fifo_full = '0') or (meta_en = '0') ) ) then
                fifo_enough <= true;
            else
                fifo_enough <= false;
            end if;
        end if;
    end process;

    -- ------------------------------------------------------------------------
    -- MINI EXP PIN SYNCHRONIZER
    -- ------------------------------------------------------------------------
    generate_sync_mimo_rx_en : for i in mini_exp'range generate
        U_sync_mini_exp : entity work.synchronizer
            generic map (
                RESET_LEVEL         =>  '0'
                )
            port map (
                reset               =>  '0',
                clock               =>  clock,
                async               =>  mini_exp(i),
                sync                =>  sync_mini_exp(i)
            );
    end generate;

    -- ------------------------------------------------------------------------
    -- META FIFO WRITER
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

        meta_future            <= meta_current;

        meta_future.meta_write <= '0';
        -- currently the GPIF modules overwrites the bottom 16 bits of the flags field
        meta_future.meta_data  <= x"FFF" & "11" & sync_mini_exp & x"FFFF" & std_logic_vector(timestamp) & x"12344321";

        case meta_current.state is
            when IDLE =>

                if ( count_enabled_channels(in_sample_controls) = 1 ) then
                    meta_future.dma_downcount <= dma_buf_size;
                else
                    meta_future.dma_downcount <= dma_buf_size / 2;
                end if;

                if( fifo_enough ) then
                    meta_future.state  <= META_WRITE;
                else
                    meta_future.meta_written <= '0';
                end if;

            when META_WRITE =>

                if( (meta_fifo_full = '0') and (in_samples(in_samples'low).data_v = '1') ) then
                    meta_future.meta_write   <= '1';
                    meta_future.meta_written <= '1';
                    meta_future.state        <= META_DOWNCOUNT;
                end if;

            when META_DOWNCOUNT =>

                meta_future.dma_downcount <= meta_current.dma_downcount - 1;

                if( meta_current.dma_downcount = 2 ) then
                    -- Look for 2 because of the 2 cycles passing
                    -- through IDLE and META_WRITE after this.
                    meta_future.state <= IDLE;
                end if;

            when others =>

                meta_future.state <= IDLE;

        end case;

        -- Abort?
        if( (enable = '0') or (meta_en = '0') ) then
            meta_future.meta_write    <= '0';
            meta_future.meta_written  <= '0';
            meta_future.state         <= IDLE;
        end if;

        -- Output assignments
        meta_fifo_write <= meta_current.meta_write;
        meta_fifo_data  <= meta_current.meta_data;

    end process;


    -- ------------------------------------------------------------------------
    -- SAMPLE FIFO WRITER
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

        -- --------------------------------------------------------------------
        -- MIMO PACKER: STEP 1 of 3
        -- --------------------------------------------------------------------
        -- This block receives samples as an array of sample streams, one
        -- element per channel. These streams need to be packed into a single,
        -- wide bus that is written to a FIFO and eventually delivered to the
        -- host. When all channels are enabled, this wide bus will contain one
        -- I/Q sample pair for each channel. When channels are disabled, the
        -- wide bus may contain multiple samples from the remaining enabled
        -- channels in order to more efficiently use the available USB bandwidth.
        -- For example, a 2x2 MIMO design with 16-bit samples will pack its data
        -- into a 64-bit wide bus in one of the following ways:
        --      | 63:48 | 47:32 | 31:16 | 15:0 | Bit indices
        --   1. |   Q1  |   I1  |   Q0  |  I0  | Channels 0 & 1 enabled
        --   2. |   Q0' |   I0' |   Q0  |  I0  | Channel 0 only enabled
        --   3. |   Q1' |   I1' |   Q1  |  I1  | Channel 1 only enabled
        function pack( sc : sample_controls_t;
                       ss : sample_streams_t;
                       d  : unsigned ) return unsigned is
            constant LEN  : natural           := ss(ss'low).data_i'length + ss(ss'low).data_q'length;
            variable rv   : unsigned(d'range) := (others => '0');
        begin
            rv := d;
            for i in sc'range loop
                if( (sc(i).enable = '1') and (ss(i).data_v = '1') ) then
                    rv := unsigned(ss(i).data_q) & unsigned(ss(i).data_i) &
                          rv(rv'high downto rv'low+LEN);
                end if;
            end loop;
            return rv;
        end function;


        variable write_req : std_logic := '0';

    begin

        fifo_future            <= fifo_current;

        fifo_future.fifo_clear <= '0';
        fifo_future.fifo_write <= '0';

        -- MIMO PACKER: STEP 1 of 3
        fifo_future.fifo_data  <= pack(in_sample_controls, in_samples, fifo_current.fifo_data);

        case fifo_current.state is

            when CLEAR =>

                -- MIMO PACKER: STEP 2 of 3
                --   Compute "samples left" to fill up the fifo_data bus
                fifo_future.samples_left      <= NUM_STREAMS - count_enabled_channels(in_sample_controls);

                if( enable = '1' ) then
                    fifo_future.fifo_clear <= '0';
                    fifo_future.state      <= WRITE_SAMPLES;
                else
                    fifo_future.fifo_clear <= '1';
                end if;

            when WRITE_SAMPLES =>

                if( ((meta_current.meta_written = '1') or (meta_en = '0')) ) then

                    -- Check for valid data
                    write_req := '0';
                    for i in in_sample_controls'range loop
                        if( in_sample_controls(i).enable = '1' ) then
                            write_req := write_req or in_samples(i).data_v;
                        end if;
                    end loop;

                    -- Received valid data
                    if( write_req = '1' ) then
                        if( fifo_current.samples_left = 0 ) then
                            fifo_future.samples_left <= NUM_STREAMS - count_enabled_channels(in_sample_controls);
                            fifo_future.fifo_write   <= write_req;
                        else
                            -- MIMO PACKER: STEP 3 of 3
                            fifo_future.samples_left <= fifo_current.samples_left - 1;
                        end if;
                    end if;
                else
                    fifo_future.fifo_write <= '0';
                end if;

                if( fifo_full = '1' or ( meta_current.meta_written = '0' and meta_en = '1') ) then
                    fifo_future.fifo_write <= '0';
                    fifo_future.state      <= HOLDOFF;
                end if;

            when HOLDOFF =>

                if( fifo_enough ) then
                    fifo_future.state <= WRITE_SAMPLES;
                end if;

            when others =>

                fifo_future.state <= CLEAR;

        end case;

        -- Abort?
        if( enable = '0' ) then
            fifo_future.fifo_clear <= '1';
            fifo_future.fifo_write <= '0';
            fifo_future.state      <= CLEAR;
        end if;

        -- Output assignments
        fifo_clear <= fifo_current.fifo_clear;
        fifo_write <= fifo_current.fifo_write;
        fifo_data  <= std_logic_vector(fifo_current.fifo_data);

    end process;


    -- ------------------------------------------------------------------------
    -- OVERFLOW
    -- ------------------------------------------------------------------------

    -- Overflow detection
    detect_overflows : process( clock, reset )
    begin
        if( reset = '1' ) then
            overflow_detected <= '0';
        elsif( rising_edge( clock ) ) then
            overflow_detected <= '0';
            if( enable = '1' and in_samples(in_samples'low).data_v = '1' and
                fifo_full = '1' and fifo_current.fifo_clear = '0' ) then
                overflow_detected <= '1';
            end if;
        end if;
    end process;

    -- Count the number of times we overflow, but only if they are discontinuous
    -- meaning we have an overflow condition, a non-overflow condition, then
    -- another overflow condition counts as 2 overflows, but an overflow condition
    -- followed by N overflow conditions counts as a single overflow condition.
    count_overflows : process( clock, reset )
        variable prev_overflow : std_logic := '0';
    begin
        if( reset = '1' ) then
            prev_overflow  := '0';
            overflow_count <= (others =>'0');
        elsif( rising_edge( clock ) ) then
            if( prev_overflow = '0' and overflow_detected = '1' ) then
                overflow_count <= overflow_count + 1;
            end if;
            prev_overflow := overflow_detected;
        end if;
    end process;

    -- Active high assertion for overflow_duration when the overflow
    -- condition has been detected.  The LED will stay asserted
    -- if multiple overflows have occurred
    blink_overflow_led : process( clock, reset )
        variable downcount : natural range 0 to 2**overflow_duration'length-1;
    begin
        if( reset = '1' ) then
            downcount    := 0;
            overflow_led <= '0';
        elsif( rising_edge(clock) ) then
            -- Default to not being asserted
            overflow_led <= '0';

            -- Countdown so we can see what happened
            if( overflow_detected = '1' ) then
                downcount := to_integer(overflow_duration);
            elsif( downcount /= 0 ) then
                downcount := downcount - 1;
                overflow_led <= '1';
            end if;
        end if;
    end process;

end architecture;
