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

entity fifo_reader is
    generic (
        NUM_STREAMS           : natural                 := 1;
        FIFO_READ_THROTTLE    : natural range 0 to 255  := 1;
        FIFO_USEDW_WIDTH      : natural                 := 12;
        FIFO_DATA_WIDTH       : natural                 := 32;
        META_FIFO_USEDW_WIDTH : natural                 := 3;
        META_FIFO_DATA_WIDTH  : natural                 := 128
    );
    port (
        clock               :   in      std_logic;
        reset               :   in      std_logic;
        enable              :   in      std_logic;

        usb_speed           :   in      std_logic;
        meta_en             :   in      std_logic;
        timestamp           :   in      unsigned(63 downto 0);

        fifo_usedw          :   in      std_logic_vector(FIFO_USEDW_WIDTH-1 downto 0);
        fifo_read           :   buffer  std_logic := '0';
        fifo_empty          :   in      std_logic;
        fifo_data           :   in      std_logic_vector(FIFO_DATA_WIDTH-1 downto 0);
        fifo_holdoff        :   in      std_logic := '0';

        meta_fifo_usedw     :   in      std_logic_vector(META_FIFO_USEDW_WIDTH-1 downto 0);
        meta_fifo_read      :   buffer  std_logic := '0';
        meta_fifo_empty     :   in      std_logic;
        meta_fifo_data      :   in      std_logic_vector(META_FIFO_DATA_WIDTH-1 downto 0);

        in_sample_controls  :   in      sample_controls_t(0 to NUM_STREAMS-1) := (others => SAMPLE_CONTROL_DISABLE);
        out_samples         :   out     sample_streams_t(0 to NUM_STREAMS-1)  := (others => ZERO_SAMPLE);

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
        COMPUTE_ENABLED_CHANNELS,
        COMPUTE_OFFSETS,
        READ_SAMPLES,
        READ_THROTTLE,
        READ_HOLDOFF
    );

    type ch_offsets_t is array( natural range <> ) of natural range fifo_data'low to fifo_data'high;

    type fifo_fsm_t is record
        state               : fifo_state_t;
        downcount           : natural range 0 to FIFO_READ_THROTTLE;
        sample_controls_reg : sample_controls_t(in_sample_controls'range);
        enabled_channels    : natural range 0 to in_sample_controls'length;
        ch_shift            : natural range 0 to out_samples'high;
        ch_offsets          : ch_offsets_t(in_sample_controls'range);
        samples_left_init   : natural range 0 to in_sample_controls'length;
        samples_left        : natural range 0 to in_sample_controls'length;
        fifo_read           : std_logic;
        out_samples         : sample_streams_t(out_samples'range);
    end record;

    constant FIFO_FSM_RESET_VALUE : fifo_fsm_t := (
        state               => COMPUTE_ENABLED_CHANNELS,
        downcount           => FIFO_READ_THROTTLE,
        sample_controls_reg => (others => SAMPLE_CONTROL_DISABLE),
        enabled_channels    => 0,
        ch_shift            => 0,
        ch_offsets          => (others => 0),
        samples_left_init   => 0,
        samples_left        => 0,
        fifo_read           => '0',
        out_samples         => (others => ZERO_SAMPLE)
    );

    signal fifo_current : fifo_fsm_t := FIFO_FSM_RESET_VALUE;
    signal fifo_future  : fifo_fsm_t := FIFO_FSM_RESET_VALUE;

begin

    -- Throw compile/synthesis error if things don't make sense
    assert ( (in_sample_controls'low = out_samples'low) and
             (in_sample_controls'high = out_samples'high) )
        report "in_sample_controls must have same range as out_samples"
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
                else
                    meta_future.meta_time_go <= '0';
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

        -- --------------------------------------------------------------------
        -- MIMO UNPACKER: STEP 1 of 5
        -- --------------------------------------------------------------------
        -- The sample FIFO output is a wide data bus that may contain more
        -- than one sample for a given channel. This function unpacks
        -- the bus into an array of sample streams. For example, a 2x2 MIMO
        -- design with 16-bit samples will pack its data into a 64-bit wide
        -- bus in one of the following ways:
        --      | 63:48 | 47:32 | 31:16 | 15:0 | Bits
        --   1. |   Q1  |   I1  |   Q0  |  I0  | Channels 0 & 1 enabled
        --   2. |   Q0' |   I0' |   Q0  |  I0  | Channel 0 only enabled
        --   3. |   Q1' |   I1' |   Q1  |  I1  | Channel 1 only enabled
        -- This function will return an array of length 2. The 0th
        -- element containing I0/Q0, and the 1st element I1/Q1. It is up
        -- to the state machine to select between element 0 and 1 based
        -- on which stream(s) is/are enabled.
        function unpack( c : sample_controls_t;
                         d : std_logic_vector ) return sample_streams_t is
            variable rv          : sample_streams_t(c'range);
            constant OFFSET_UNIT : natural := rv(rv'low).data_i'length +
                                              rv(rv'low).data_q'length;
            -- The following 4 constants are platform-specific and perhaps
            -- should be parameters instead. This is good enough for now.
            constant I_HIGH      : natural := 11;
            constant I_LOW       : natural := 0;
            constant Q_HIGH      : natural := 27;
            constant Q_LOW       : natural := 16;
        begin
            -- Ensure the array indices are normalized
            assert (c'low = 0) and (c'high >= c'low)
                report "Invalid range for parameter 'c'"
                severity failure;

            for i in rv'range loop
                rv(i).data_i := resize(signed(shift_right(unsigned(d),i*OFFSET_UNIT)(I_HIGH downto I_LOW)),rv(i).data_i'length);
                rv(i).data_q := resize(signed(shift_right(unsigned(d),i*OFFSET_UNIT)(Q_HIGH downto Q_LOW)),rv(i).data_q'length);
                rv(i).data_v := '0';
            end loop;

            return rv;
        end function;

        -- --------------------------------------------------------------------
        -- MIMO UNPACKER: STEP 3 of 5
        -- --------------------------------------------------------------------
        -- The FIFO data has been unpacked into an array containing I and Q
        -- samples for each of the possible channels they belong to. We need to
        -- figure out which index of this unpacked array belongs to which
        -- channel so we can output it to the correct endpoint, as follows:
        --   a. All channel indices start at 0.
        --   b. For each channel, add 1 to the index for each previous
        --      channel that is enabled. For 2x2 MIMO:
        --        ch_enabled | array index of the unpacked data stream
        --        0 & 1      | ch0 = 0    ; ch1 = 0 + 1 = 1
        --        0 only     | ch0 = 0    ; ch1 = -     = 0 (ch1 is disabled, doesn't matter)
        --        1 only     | ch0 = - = 0; ch1 = 0 + 0 = 0 (ch0 is disabled, doesn't matter)
        function compute_initial_channel_offsets( x : sample_controls_t ) return ch_offsets_t is
            variable rv : ch_offsets_t(x'range) := (others => 0);
        begin
            rv := (others => 0);
            for i in x'low+1 to x'high loop
                for j in x'low to x'high-1 loop
                    if( x(j).enable = '1' ) then
                        rv(i) := rv(i) + 1;
                    end if;
                end loop;
            end loop;
            return rv;
        end function;


        variable unpacked         : sample_streams_t(out_samples'range);
        variable read_req         : std_logic                     := '0';

    begin

        fifo_future <= fifo_current;

        fifo_future.fifo_read <= '0';

        -- MIMO UNPACKER: STEP 1 of 5
        unpacked := unpack(fifo_current.sample_controls_reg, fifo_data);
        for i in fifo_future.out_samples'range loop
            if( fifo_current.sample_controls_reg(i).enable = '1' ) then
                fifo_future.out_samples(i) <= unpacked(fifo_current.ch_offsets(i) + fifo_current.ch_shift);
            end if;
        end loop;

        case fifo_current.state is

            when COMPUTE_ENABLED_CHANNELS =>

                -- MIMO UNPACKER: STEP 2 of 5
                --   Count the number of enabled channels
                fifo_future.enabled_channels <= count_enabled_channels(in_sample_controls);

                -- Register the sample control settings
                fifo_future.sample_controls_reg <= in_sample_controls;

                fifo_future.state <= COMPUTE_OFFSETS;

            when COMPUTE_OFFSETS =>

                -- MIMO UNPACKER: STEP 3 of 5
                fifo_future.ch_offsets <= compute_initial_channel_offsets(fifo_current.sample_controls_reg);

                -- MIMO UNPACKER: STEP 4 of 5
                --   Compute the number of valid samples that each channel has remaining in fifo_data that
                --   still need to be processed (not including the first). This becomes the number of clock
                --   cycles to wait before asserting the FIFO read request to get a new batch of samples.
                fifo_future.samples_left_init <= NUM_STREAMS - fifo_current.enabled_channels;
                fifo_future.samples_left      <= NUM_STREAMS - fifo_current.enabled_channels;

                fifo_future.state <= READ_SAMPLES;

            when READ_SAMPLES =>

                fifo_future.downcount <= FIFO_FSM_RESET_VALUE.downcount;

                if( fifo_holdoff = '1' ) then
                    -- Pause for a spell
                    fifo_future.state <= READ_HOLDOFF;

                elsif( fifo_empty = '0' and
                       (meta_en = '0' or (meta_en = '1' and meta_current.meta_time_go = '1')) ) then

                    -- Check for valid data request
                    read_req := '0';
                    for i in in_sample_controls'range loop
                        read_req := read_req or (in_sample_controls(i).data_req and in_sample_controls(i).enable);
                        fifo_future.out_samples(i).data_v <= in_sample_controls(i).data_req and
                                                             in_sample_controls(i).enable;
                    end loop;

                    -- Received a valid data request
                    if( read_req = '1' ) then
                        if( fifo_current.samples_left = 0 ) then
                            fifo_future.samples_left <= fifo_current.samples_left_init;
                            fifo_future.ch_shift     <= 0;
                            fifo_future.fifo_read    <= read_req;
                        else
                            -- MIMO UNPACKER: STEP 5 of 5
                            --   Add the number of enabled channels to each channel's offset index.
                            --   This points that channel to the next valid sample in the current fifo_data.
                            fifo_future.ch_shift     <= fifo_current.ch_shift + fifo_current.enabled_channels;
                            fifo_future.samples_left <= fifo_current.samples_left - 1;
                        end if;

                        if( FIFO_FSM_RESET_VALUE.downcount /= 0 ) then
                            fifo_future.state <= READ_THROTTLE;
                        end if;
                    end if;

                end if;

            when READ_THROTTLE =>

                -- If in this state, downcount is guaranteed to be >= 1
                if( fifo_current.downcount = 1 ) then
                    fifo_future.state     <= READ_SAMPLES;
                else
                    fifo_future.downcount <= fifo_current.downcount - 1;
                end if;

            when READ_HOLDOFF =>

                if( fifo_holdoff = '0' ) then
                    fifo_future.state <= READ_SAMPLES;
                end if;

            when others =>

                fifo_future.state <= FIFO_FSM_RESET_VALUE.state;

        end case;

        -- Abort?
        if( enable = '0' ) then
            fifo_future.fifo_read <= '0';
            fifo_future.state     <= FIFO_FSM_RESET_VALUE.state;
            for i in fifo_current.out_samples'range loop
                fifo_future.out_samples(i).data_v <= '0';
            end loop;
        end if;

        if( fifo_empty = '1' ) then
            -- Re-evaluate the MIMO settings
            fifo_future.state <= FIFO_FSM_RESET_VALUE.state;
        end if;

        -- Output assignments
        fifo_read   <= fifo_current.fifo_read;
        out_samples <= fifo_current.out_samples;

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
