-- Copyright (c) 2019 Nuand LLC
--
-- LICENSE TBD

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

library work;
    use work.tone_generator_p.all;

-- Register map:
--  0   status/control
--              write           read
--      bit 0:  irq_enable      irq_en
--      bit 1:  irq_disable     irq_pending
--      bit 2:  append          queue_empty
--      bit 3:  clear           queue_full
--      bit 4:                  tone active
--      bit 5:                  control operation pending
--      bit 6:                  last control op failed
--      others: reserved/undef
--  1   dphase - phase rotation per clock cycle (signed)
--      one full rotation is 8192, so:
--          dphase = 8192 / (f_clock / f_tone)
--  2   duration - in clocks (unsigned)

entity tone_generator_hw is
    generic (
        QUEUE_LENGTH    : positive  := 16;
        DEFAULT_IRQ_EN  : boolean   := false
    );
    port (
        -- Control signals
        clock       : in    std_logic;
        reset       : in    std_logic;

        -- Tone generator interface
        tgen_out    : out   tone_generator_input_t;
        tgen_in     : in    tone_generator_output_t;

        -- Memory mapped interface
        addr        : in    std_logic_vector(3 downto 0);
        din         : in    std_logic_vector(31 downto 0);
        dout        : out   std_logic_vector(31 downto 0);
        write       : in    std_logic;
        read        : in    std_logic;
        waitreq     : out   std_logic;
        readack     : out   std_logic;
        intr        : out   std_logic
    );
end entity tone_generator_hw;

architecture arch of tone_generator_hw is
--------------------------------------------------------------------------------
-- Types
--------------------------------------------------------------------------------
    constant NUM_REGS : natural := 3;

    type tone_entries_t is array(natural range <>) of tone_generator_input_t;

    type tone_queue_t is record
        entries  : tone_entries_t(0 to QUEUE_LENGTH-1);
        ins_idx  : natural range 0 to QUEUE_LENGTH-1;
        rem_idx  : natural range 0 to QUEUE_LENGTH-1;
        -- if ins_idx = rem_idx, it could mean one of two things:
        -- the queue is empty, or the queue is full
        -- so we track this case here...
        overflow : boolean;
    end record tone_queue_t;

    type fsm_t is (INIT, IDLE, HANDLE_CONTROL_OP, CLEAR_OP_PENDING, QUEUE_CLEAR,
                   QUEUE_PUSH, QUEUE_POP, GENERATE_TONE, SET_IRQ);

    type state_t is record
        state           : fsm_t;
        queue           : tone_queue_t;
        tone            : tone_generator_input_t;
        irq_en          : boolean;
        irq_set         : boolean;
        irq_done        : boolean;
        tone_active     : boolean;
        op_pending      : boolean;
        op_failed      : boolean;
    end record state_t;

    type register_t  is array(natural range din'range) of std_logic;
    type registers_t is array(natural range 0 to NUM_REGS-1) of register_t;

    type control_reg_t is record
        irq_enable  : boolean;
        irq_disable : boolean;
        enqueue     : boolean;
        clear       : boolean;
    end record control_reg_t;

--------------------------------------------------------------------------------
-- Signals
--------------------------------------------------------------------------------
    signal current      : state_t;
    signal future       : state_t;

    signal current_regs : registers_t;
    signal future_regs  : registers_t;

    signal pending_ctrl : boolean;

    signal uaddr        : natural range 0 to 2**addr'high;

    signal irq_clr      : boolean;
    signal intr_i       : std_logic;

--------------------------------------------------------------------------------
-- Subprograms
--------------------------------------------------------------------------------
    function to_slv (val : register_t) return std_logic_vector is
    begin
        return std_logic_vector(unsigned(val));
    end function to_slv;

    function to_reg (val : std_logic_vector) return register_t is
    begin
        return register_t(val);
    end function to_reg;

    function get_queue_len (queue : tone_queue_t) return natural is
    begin
        -- sanity check:
        -- given QUEUE_LENGTH is 8,
        --  overflow    ins_idx     rem_idx     return
        --      f          0           0           0
        --      f          7           7           0
        --      t          0           0           8
        --      t          4           4           8
        --      _          5           3           2
        --      _          2           4           6
        --      _          7           0           7

        if (queue.overflow) then
            return QUEUE_LENGTH;
        elsif (queue.ins_idx >= queue.rem_idx) then
            return queue.ins_idx - queue.rem_idx;
        else -- queue.ins_idx < queue.rem_idx
            return QUEUE_LENGTH - queue.rem_idx + queue.ins_idx;
        end if;
    end function get_queue_len;

    function next_index (idx : natural) return natural is
    begin
        return (idx + 1) mod QUEUE_LENGTH;
    end function next_index;

    procedure queue_append (signal q_in   : in  tone_queue_t;
                            signal q_out  : out tone_queue_t;
                            constant tone : in  tone_generator_input_t;
                            success       : out boolean) is
        variable ql : natural;
    begin
        q_out   <= q_in;
        ql      := get_queue_len(q_in);
        success := true;

        if (ql >= QUEUE_LENGTH) then
            -- queue is full
            success := false;
        else
            q_out.entries(q_in.ins_idx) <= tone;
            q_out.ins_idx               <= next_index(q_in.ins_idx);
        end if;

        q_out.overflow <= ((ql + 1) >= QUEUE_LENGTH);
    end procedure queue_append;

    procedure queue_dequeue (signal q_in  : in  tone_queue_t;
                             signal q_out : out tone_queue_t;
                             signal tone  : out tone_generator_input_t;
                             success      : out boolean) is
        variable ql : natural;
    begin
        q_out   <= q_in;
        ql      := get_queue_len(q_in);
        success := true;

        if (ql = 0) then
            -- queue is empty
            success := false;
        else
            tone          <= q_in.entries(q_in.rem_idx);
            q_out.rem_idx <= next_index(q_in.rem_idx);
        end if;

        q_out.overflow <= false;
    end procedure queue_dequeue;

    function get_status_reg (state : state_t) return register_t is
        constant queue  : tone_queue_t := state.queue;
        variable rv     : register_t;
    begin
        rv    := (others => '0');
        rv(0) := '1' when state.irq_en else '0';
        rv(1) := '1' when state.irq_set else '0';
        rv(2) := '1' when (get_queue_len(queue) = 0) else '0';
        rv(3) := '1' when (get_queue_len(queue) = QUEUE_LENGTH) else '0';
        rv(4) := '1' when state.tone_active else '0';
        rv(5) := '1' when state.op_pending else '0';
        rv(6) := '1' when state.op_failed else '0';
        return rv;
    end function get_status_reg;

    function get_control_reg (reg : register_t) return control_reg_t is
        variable rv : control_reg_t;
    begin
        rv.irq_enable   := (reg(0) = '1');
        rv.irq_disable  := (reg(1) = '1');
        rv.enqueue      := (reg(2) = '1');
        rv.clear        := (reg(3) = '1');
        return rv;
    end function get_control_reg;

    function NULL_REGISTERS return registers_t is
        variable rv : registers_t;
    begin
        rv := (others => (others => '0'));
        return rv;
    end function NULL_REGISTERS;

    function NULL_TONE_QUEUE return tone_queue_t is
        variable rv : tone_queue_t;
    begin
        rv.entries  := (others => NULL_TONE_GENERATOR_INPUT);
        rv.ins_idx  := 0;
        rv.rem_idx  := 0;
        rv.overflow := false;
        return rv;
    end function NULL_TONE_QUEUE;

    function NULL_STATE return state_t is
        variable rv : state_t;
    begin
        rv.state        := INIT;
        rv.queue        := NULL_TONE_QUEUE;
        rv.tone         := NULL_TONE_GENERATOR_INPUT;
        rv.irq_en       := DEFAULT_IRQ_EN;
        rv.irq_set      := false;
        rv.irq_done     := false;
        rv.tone_active  := false;
        rv.op_pending   := false;
        rv.op_failed   := true;
        return rv;
    end function NULL_STATE;

begin

    waitreq <= '1' when current.op_pending else '0';
    uaddr   <= to_integer(unsigned(addr));
    intr    <= intr_i;

    mm_read : process(clock)
    begin
        if (reset = '1') then
            dout    <= (others => '0');
            readack <= '0';
            irq_clr <= false;
        elsif (rising_edge(clock)) then
            readack <= '0';
            irq_clr <= false;

            if (read = '1') then
                readack <= '1';
            end if;

            if (uaddr = 0) then
                dout    <= to_slv(get_status_reg(current));
                irq_clr <= true;
            else
                dout <= to_slv(current_regs(uaddr)) when (uaddr < NUM_REGS)
                                                    else (others => 'X');
            end if;
        end if;
    end process mm_read;

    mm_write : process(clock, reset)
    begin
        if (reset = '1') then
            future_regs <= (others => (others => '0'));
            pending_ctrl <= false;
        elsif (rising_edge(clock)) then
            if (write = '1' and uaddr < NUM_REGS) then
                future_regs(uaddr) <= to_reg(din);

                if (uaddr = 0) then
                    pending_ctrl <= true;
                end if;
            end if;

            if (current.op_pending = true) then
                pending_ctrl <= false;
            end if;
        end if;
    end process mm_write;

    mm_intr : process(clock, reset)
    begin
        if (reset = '1') then
            intr_i <= '0';
        elsif (rising_edge(clock)) then
            if (irq_clr) then
                intr_i <= '0';
            end if;

            if (current.irq_set) then
                intr_i <= '1';
            end if;
        end if;
    end process mm_intr;

    sync_proc : process(clock, reset)
    begin
        if (reset = '1') then
            current         <= NULL_STATE;
            current_regs    <= NULL_REGISTERS;
        elsif (rising_edge(clock)) then
            current         <= future;
            current_regs    <= future_regs;
        end if;
    end process sync_proc;

    comb_proc : process(all)
        variable ctrl_reg : control_reg_t;
        variable tone_val : tone_generator_input_t;
        variable success : boolean;
    begin
        future <= current;

        future.op_pending   <= current.op_pending or pending_ctrl;
        future.tone_active  <= (tgen_in.active = '1');
        future.irq_set      <= false;

        case (current.state) is
            when INIT =>
                future.state <= IDLE;

            when IDLE =>
                future.state <= IDLE;

                if (current.op_pending) then
                    future.state <= HANDLE_CONTROL_OP;
                end if;

                if (get_queue_len(current.queue) > 0) then
                    -- there is work to do
                    if (not current.tone_active) then
                        -- let's do it!
                        future.irq_done <= false;
                        future.state <= QUEUE_POP;
                    end if;
                else
                    -- there is no work to do
                    if (not current.irq_done) then
                        -- let's report such!
                        future.state <= SET_IRQ;
                    end if;
                end if;

            when HANDLE_CONTROL_OP =>
                future.state <= CLEAR_OP_PENDING;

                ctrl_reg := get_control_reg(current_regs(0));

                -- irq_enable  irq_disable  irq_en
                --     0            0        N/C
                --     0            1         0
                --     1            0         1
                --     1            1        N/C
                if (ctrl_reg.irq_enable /= ctrl_reg.irq_disable) then
                    future.irq_en <= ctrl_reg.irq_enable or
                                     not ctrl_reg.irq_disable;
                end if;

                if (ctrl_reg.enqueue) then
                    future.state <= QUEUE_PUSH;
                end if;

                -- clear overrides enqueue
                if (ctrl_reg.clear) then
                    future.state <= QUEUE_CLEAR;
                end if;

            when CLEAR_OP_PENDING =>
                future.state      <= IDLE;
                future.op_pending <= false;

            when QUEUE_CLEAR =>
                future.state     <= CLEAR_OP_PENDING;
                future.queue     <= NULL_TONE_QUEUE;
                future.op_failed <= false;

            when QUEUE_PUSH =>
                future.state <= CLEAR_OP_PENDING;

                tone_val.dphase   := to_integer(signed(current_regs(1)));
                tone_val.duration := to_integer(unsigned(current_regs(2)));
                tone_val.valid    := '1';

                queue_append(current.queue, future.queue, tone_val, success);

                future.op_failed <= not success;                    

            when QUEUE_POP =>
                future.state <= GENERATE_TONE;

                queue_dequeue(current.queue, future.queue, future.tone, success);

                if (not success) then
                    -- oh no
                    future.op_failed <= true;
                    future.state     <= IDLE;
                end if;

            when GENERATE_TONE =>
                future.state <= IDLE;
                tgen_out     <= current.tone;

            when SET_IRQ =>
                if (current.irq_en) then
                    future.irq_set <= true;
                end if;

                future.irq_done <= true;
                future.state <= IDLE;

        end case;
    end process comb_proc;

end architecture arch;
