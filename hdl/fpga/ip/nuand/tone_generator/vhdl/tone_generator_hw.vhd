-- Copyright (c) 2019 Nuand LLC
--
-- LICENSE TBD

-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

library work;
    use work.tone_generator_p.all;

-- Register map:
--  0   control/status
--          control (write):
--              bit 0:  append
--              bit 1:  clear
--          status (read):
--              bit 0:  running
--              bit 1:  queue full
--              bits 8-15: number of items in the queue (unsigned)
--      others: reserved/undef
--  1   dphase - phase rotation per clock cycle (signed)
--      one full rotation is 8192, and this is signed, so:
--          dphase = 4096 / (f_clock / f_tone)
--  2   duration - in clocks (unsigned)

entity tone_generator_hw is
    generic (
        QUEUE_LENGTH    : positive  := 8;
        ADDR_WIDTH      : positive  := 2;
        DATA_WIDTH      : positive  := 32
    );
    port (
        -- Control signals
        clock           : in    std_logic;
        reset           : in    std_logic;

        -- Samples
        sample_clk      : in    std_logic;
        sample_i        : out   signed(15 downto 0);
        sample_q        : out   signed(15 downto 0);
        sample_valid    : out   std_logic;

        -- Memory mapped interface
        addr            : in    std_logic_vector(ADDR_WIDTH-1 downto 0);
        din             : in    std_logic_vector(DATA_WIDTH-1 downto 0);
        dout            : out   std_logic_vector(DATA_WIDTH-1 downto 0);
        write           : in    std_logic;
        read            : in    std_logic;
        waitreq         : out   std_logic;
        readack         : out   std_logic;

        -- Status indicators
        queue_empty     : out   std_logic;
        queue_full      : out   std_logic;
        running         : out   std_logic
    );
end entity tone_generator_hw;

architecture arch of tone_generator_hw is
--------------------------------------------------------------------------------
-- Types
--------------------------------------------------------------------------------
    constant NUM_REGS : natural := 3;

    type tone_entries_t is array(natural range <>) of tone_generator_input_t;

    type tone_queue_t is record
        entries     : tone_entries_t(0 to QUEUE_LENGTH-1);
        front       : natural range  0 to QUEUE_LENGTH-1; -- next pop
        rear        : natural range  0 to QUEUE_LENGTH-1; -- last push
        count       : natural range  0 to QUEUE_LENGTH;   -- items in queue
    end record tone_queue_t;

    type fsm_t is (IDLE, CTRL_DISPATCH, CTRL_CLEAR, QUEUE_CLEAR, QUEUE_PUSH,
                   QUEUE_POP, TONE_GENERATE, WAIT_FOR_ACTIVE);

    type state_t is record
        state       : fsm_t;
        queue       : tone_queue_t;
        tone        : tone_generator_input_t;
        op_pending  : boolean;
    end record state_t;

    type register_t  is array(natural range din'range) of std_logic;
    type registers_t is array(natural range 0 to NUM_REGS-1) of register_t;

    type control_reg_t is record
        append      : boolean;
        clear       : boolean;
    end record control_reg_t;

--------------------------------------------------------------------------------
-- Signals
--------------------------------------------------------------------------------
    signal current      : state_t;
    signal future       : state_t;

    signal current_regs : registers_t;
    signal future_regs  : registers_t;

    signal pend_ctrl    : boolean;

    signal uaddr        : natural range 0 to 2**addr'high;

    signal tgen_in      : tone_generator_input_t;
    signal tgen_out     : tone_generator_output_t;
    signal tgen_reset   : std_logic;

    signal tgen_out_a   : std_logic;
    signal tgen_out_v   : std_logic;

    signal q_full_i     : std_logic;
    signal q_empty_i    : std_logic;
    signal running_i    : std_logic;

--------------------------------------------------------------------------------
-- Subprograms
--------------------------------------------------------------------------------
    function to_slv (val : register_t) return std_logic_vector is
    begin
        return std_logic_vector(unsigned(val));
    end function to_slv;

    function to_sl (val : boolean) return std_logic is
    begin
        if (val) then
            return '1';
        else
            return '0';
        end if;
    end function to_sl;

    function to_reg (val : std_logic_vector) return register_t is
    begin
        return register_t(val);
    end function to_reg;

    function next_index (idx : natural) return natural is
    begin
        return (idx + 1) mod QUEUE_LENGTH;
    end function next_index;

    procedure queue_push (signal q_in   : in  tone_queue_t;
                          signal q_out  : out tone_queue_t;
                          constant tone : in  tone_generator_input_t;
                          success       : out boolean) is
        variable next_i : natural;
        variable ql     : natural;
    begin
        q_out   <= q_in;
        next_i  := next_index(q_in.rear);
        ql      := q_in.count;

        if (q_in.count >= QUEUE_LENGTH) then
            -- queue is full
            success := false;
        else
            -- add the entry at the NEXT rear index
            q_out.entries(next_i) <= tone;

            -- update indexes and counts
            ql          := q_in.count + 1;
            q_out.count <= ql;
            q_out.rear  <= next_i;
            success     := true;
        end if;

        --report "queue_push:"    &
        --       " prev_rear="    & to_string(q_in.rear) &
        --       " this_rear="    & to_string(next_i) &
        --       " prev_count="   & to_string(q_in.count) &
        --       " this_count="   & to_string(ql) &
        --       " success="      & to_string(success);
    end procedure queue_push;

    procedure queue_pop (signal q_in  : in  tone_queue_t;
                         signal q_out : out tone_queue_t;
                         signal tone  : out tone_generator_input_t;
                         success      : out boolean) is
        variable next_i : natural;
        variable ql     : natural;
    begin
        q_out   <= q_in;
        next_i  := next_index(q_in.front);
        ql      := q_in.count;

        if (q_in.count = 0) then
            -- queue is empty
            success := false;
        else
            -- pop the entry at the CURRENT front index
            tone <= q_in.entries(q_in.front);

            -- update indexes and counts
            ql          := q_in.count - 1;
            q_out.count <= ql;
            q_out.front <= next_i;
            success     := true;
        end if;

        --report "queue_pop:"     &
        --       " prev_front="   & to_string(q_in.front) &
        --       " this_front="   & to_string(next_i) &
        --       " prev_count="   & to_string(q_in.count) &
        --       " this_count="   & to_string(ql) &
        --       " success="      & to_string(success);
    end procedure queue_pop;

    function unpack_control (reg : register_t) return control_reg_t is
        variable rv : control_reg_t;
    begin
        rv.append   := (reg(0) = '1');
        rv.clear    := (reg(1) = '1');
        return rv;
    end function unpack_control;

    function unpack_control (reg : std_logic_vector) return control_reg_t is
    begin
        return unpack_control(to_reg(reg));
    end function unpack_control;

    function NULL_REGISTERS return registers_t is
    begin
        return (others => (others => '0'));
    end function NULL_REGISTERS;

    function NULL_TONE_QUEUE return tone_queue_t is
        variable rv : tone_queue_t;
    begin
        rv.entries  := (others => NULL_TONE_GENERATOR_INPUT);
        rv.front    := 0;
        rv.rear     := QUEUE_LENGTH-1;
        return rv;
    end function NULL_TONE_QUEUE;

    function NULL_STATE return state_t is
        variable rv : state_t;
    begin
        rv.state        := IDLE;
        rv.queue        := NULL_TONE_QUEUE;
        rv.tone         := NULL_TONE_GENERATOR_INPUT;
        rv.op_pending   := false;
        return rv;
    end function NULL_STATE;

begin
    -- Output mappings
    sample_i        <= tgen_out.re;
    sample_q        <= tgen_out.im;
    sample_valid    <= tgen_out.valid;
    queue_full      <= q_full_i;
    queue_empty     <= q_empty_i;
    running         <= running_i;

    -- For convenience
    uaddr <= to_integer(unsigned(addr));

    -- Status indicators
    q_empty_i   <= '1' when (current.queue.count = 0) else '0';
    q_full_i    <= '1' when (current.queue.count >= QUEUE_LENGTH) else '0';
    running_i   <= '1' when (tgen_out_a = '1' or tgen_out_v = '1') else '0';

    -- Tone generator interface (with clock domain crossing assist)
    -- reset, tgen_in: system clock -> sample clock
    -- tgen_out: sample clock -> system clock
    U_reset_sync : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL  =>  '1',
            OUTPUT_LEVEL =>  '1'
        )
        port map (
            clock   => sample_clk,
            async   => reset,
            sync    => tgen_reset
        );

    U_tgen_in_valid_sync : entity work.synchronizer
        generic map (
            RESET_LEVEL => '0'
        )
        port map (
            reset   => tgen_reset,
            clock   => sample_clk,
            async   => current.tone.valid,
            sync    => tgen_in.valid
        );

    generate_sync_tgen_in_dphase : for i in tgen_in.dphase'range generate
        U_tgen_in_dphase : entity work.synchronizer
            generic map (
                RESET_LEVEL => '0'
            )
            port map (
                reset   =>  tgen_reset,
                clock   =>  sample_clk,
                async   =>  current.tone.dphase(i),
                sync    =>  tgen_in.dphase(i)
            );
    end generate;

    generate_sync_tgen_in_duration : for i in tgen_in.duration'range generate
        U_tgen_in_duration : entity work.synchronizer
            generic map (
                RESET_LEVEL => '0'
            )
            port map (
                reset   =>  tgen_reset,
                clock   =>  sample_clk,
                async   =>  current.tone.duration(i),
                sync    =>  tgen_in.duration(i)
            );
    end generate;

    U_tgen_out_valid_sync : entity work.synchronizer
        port map (
            reset   => reset,
            clock   => clock,
            async   => tgen_out.valid,
            sync    => tgen_out_v
        );

    U_tgen_out_active_sync : entity work.synchronizer
        port map (
            reset   => reset,
            clock   => clock,
            async   => tgen_out.active,
            sync    => tgen_out_a
        );

    U_tone_generator : entity work.tone_generator
        port map (
            clock   => sample_clk,
            reset   => tgen_reset,
            inputs  => tgen_in,
            outputs => tgen_out
        );

    -- Avalon-MM interface
    waitreq <= '1' when (current.op_pending or pend_ctrl) else '0';

    mm_read : process(clock, reset)
    begin
        if (reset = '1') then
            readack <= '0';
            dout    <= (others => '0');

        elsif (rising_edge(clock)) then
            -- hold off ack until there's no ops pending (race condition)
            readack <= to_sl(read = '1' and not current.op_pending);

            if (uaddr = 0) then
                -- count of items in queue
                dout <= (others => '0');
                dout(0) <= running_i;
                dout(1) <= q_full_i;
                dout(15 downto 8) <=
                        std_logic_vector(to_unsigned(current.queue.count, 8));
            else
                dout <= to_slv(current_regs(uaddr));
            end if;
        end if;
    end process mm_read;

    mm_write : process(clock, reset)
    begin
        if (reset = '1') then
            future_regs <= (others => (others => '0'));
            pend_ctrl   <= false;

        elsif (rising_edge(clock)) then
            if (current.op_pending) then
                -- our previous pend_ctrl has been acknowledged
                pend_ctrl <= false;
            end if;

            if (write = '1' and uaddr < NUM_REGS) then
                future_regs(uaddr) <= to_reg(din);

                if (uaddr = 0) then
                    -- send a pend_ctrl signal for the state machine to pick up
                    pend_ctrl <= true;
                end if;
            end if;
        end if;
    end process mm_write;

    -- State machinery
    sync_proc : process(clock, reset)
    begin
        if (reset = '1') then
            current      <= NULL_STATE;
            current_regs <= NULL_REGISTERS;

        elsif (rising_edge(clock)) then
            current      <= future;
            current_regs <= future_regs;
        end if;
    end process sync_proc;

    comb_proc : process(all)
        variable ctrl_reg : control_reg_t;
        variable tone_val : tone_generator_input_t := NULL_TONE_GENERATOR_INPUT;
        variable success  : boolean := true;
    begin
        future <= current;

        future.op_pending <= current.op_pending or pend_ctrl;
        future.tone.valid <= '0';

        tone_val := NULL_TONE_GENERATOR_INPUT;

        case (current.state) is
            when IDLE =>
                if (current.op_pending) then
                    -- we have a control message to deal with
                    future.state <= CTRL_DISPATCH;
                end if;

                if (current.queue.count > 0) then
                    -- there is work to do
                    if (tgen_out_a = '0') then
                        -- let's do it!
                        future.state <= QUEUE_POP;
                    end if;
                end if;

            when CTRL_DISPATCH =>
                ctrl_reg := unpack_control(current_regs(0));

                -- default
                future.state <= CTRL_CLEAR;

                -- append
                if (ctrl_reg.append) then
                    future.state <= QUEUE_PUSH;
                end if;

                -- clear (overrides append)
                if (ctrl_reg.clear) then
                    future.state <= QUEUE_CLEAR;
                end if;

            when CTRL_CLEAR =>
                future.op_pending <= false;
                future.state      <= IDLE;

            when QUEUE_CLEAR =>
                future.queue <= NULL_TONE_QUEUE;
                future.state <= CTRL_CLEAR;

            when QUEUE_PUSH =>
                tone_val.dphase
                    := signed(current_regs(1)(tone_val.dphase'range));
                tone_val.duration
                    := unsigned(current_regs(2)(tone_val.duration'range));

                queue_push(current.queue, future.queue, tone_val, success);

                assert success report "QUEUE_PUSH failed" severity failure;

                future.state <= CTRL_CLEAR;

            when QUEUE_POP =>
                queue_pop(current.queue, future.queue, future.tone, success);

                assert success report "QUEUE_POP failed" severity failure;

                future.state <= TONE_GENERATE;

                if (not success) then
                    future.state <= IDLE;
                end if;

            when TONE_GENERATE =>
                future.tone.valid <= '1';
                future.state      <= WAIT_FOR_ACTIVE;

            when WAIT_FOR_ACTIVE =>
                future.tone.valid <= '1';

                if (tgen_out_a = '1') then
                    future.tone.valid <= '0';
                    future.state      <= IDLE;
                end if;

        end case;
    end process comb_proc;

end architecture arch;
