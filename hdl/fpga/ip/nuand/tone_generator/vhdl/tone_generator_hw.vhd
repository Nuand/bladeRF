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
--      bit 0: irq_enable
--      bit 1: write=append, read=empty
--      bit 2: write=clear, read=full
--      others: reserved/undef
--  1   dphase - phase rotation per clock cycle
--      one full rotation is 8192, so:
--          dphase = 8192 / (f_clock / f_tone)
--  2   duration - in clocks

entity tone_generator_hw is
    generic (
        QUEUE_LENGTH : positive := 16
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
end entity;

architecture arch of tone_generator_hw is
    type fsm_t is (INIT, IDLE, CLEAR_QUEUE, APPEND_TO_QUEUE, CHECK_QUEUE,
                   GENERATE_TONE, SET_IRQ);

    type tone_entries_t is array(natural range <>) of tone_generator_input_t;

    type tone_queue_t is record
        entries  : tone_entries_t(0 to QUEUE_LENGTH-1);
        ins_idx  : natural range 0 to QUEUE_LENGTH-1;
        rem_idx  : natural range 0 to QUEUE_LENGTH-1;
        -- if ins_idx = rem_idx, it could mean one of two things:
        -- the queue is empty, or the queue is full
        -- so we track this case here...
        overflow : boolean;
    end record;

    constant NULL_TONE_QUEUE : tone_queue_t := (
        entries  => (others => NULL_TONE_GENERATOR_INPUT),
        ins_idx  => 0,
        rem_idx  => 0,
        overflow => false
    );

    type state_t is record
        state   : fsm_t;
        queue   : tone_queue_t;
        tone    : tone_generator_input_t;
    end record;

    type register_t is std_logic_vector(din'range);
    type registers_t is array(natural range <>) of register_t;

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

        if (queue.ins_idx = queue.rem_idx) then
            if (queue.overflow) then
                return QUEUE_LENGTH;
            else
                return queue.ins_idx - queue.rem_idx;
            end if;
        elsif (queue.ins_idx > queue.rem_idx) then
            return queue.ins_idx - queue.rem_idx;
        else -- queue.ins_idx < queue.rem_idx
            return QUEUE_LENGTH - queue.rem_idx + queue.ins_idx;
        end if;
    end function get_queue_len;

    procedure queue_append (signal queue : inout    tone_queue_t,
                            tone         : in       tone_generator_input_t,
                            success      : out      boolean) is
        variable ql : natural;
    begin
        ql := get_queue_len(queue);

        if (ql >= QUEUE_LENGTH) then
            -- oh no
            success := false;
        else
            queue.entries(queue.ins_idx) <= tone;
            queue.ins_idx <= (queue.ins_idx + 1) mod QUEUE_LENGTH;
            success := true;
        end if;

        queue.overflow <= ((ql + 1) >= QUEUE_LENGTH);
    end procedure queue_append;

    procedure queue_dequeue (signal queue : inout   tone_queue_t,
                             tone         : out     tone_generator_input_t,
                             success      : out     boolean) is
        variable ql : natural;
    begin
        ql := get_queue_len(queue);

        if (ql = 0) then
            -- queue's empty
            success := false;
        else
            tone <= queue.entries(queue.rem_idx);
            queue.rem_idx <= (queue.rem_idx + 1) mod QUEUE_LENGTH;
            queue.overflow <= false;
            success := true;
        end if;
    end procedure queue_dequeue;

    function get_status_reg return register_t is
        variable rv : register_t;
    begin
        rv    := reg_space(0);
        rv(1) := (get_queue_len(queue) = 0);
        rv(2) := (get_queue_len(queue) = QUEUE_LENGTH);

        return rv;
    end function get_status_reg;

    function NULL_STATE return state_t is
        variable rv : state_t;
    begin
        rv.state := INIT;
        rv.queue := NULL_TONE_QUEUE;
        rv.tone  := NULL_TONE_GENERATOR_INPUT;
        return rv;
    end function;

    signal current : state_t;
    signal future  : state_t;

    signal reg_space : registers_t(0 to 2) := (others => (others => '0'));
    signal uaddr     : unsigned(addr'range);
    signal readack_i : std_logic;
begin

    waitreq <= '0';
    readack <= readack_i;

    uaddr <= unsigned(addr);

    generate_ack : process(clock, reset)
    begin
        if (reset = '1') then
            readack_i <= '0';
        elsif (rising_edge(clock)) then
            if (read = '1') then
                readack_i <= '1';
            else
                readack_i <= '0';
            end if;
        end if;
    end process generate_ack;

    mm_read : process(clock)
    begin
        if (rising_edge(clock)) then
            case to_integer(uaddr) is
                when 0  => dout <= get_status_reg();
                when others  => dout <= reg_space(uaddr);
            end case;
        end if;
    end process mm_read;

    mm_write : process(clock, reset)
    begin
        if (reset = '1') then
            -- write me
        elsif (rising_edge(clock)) then
            if (write = '1') then
                case to_integer(uaddr) is
                    when others => null;
                end case;
            end if;
        end if;
    end process mm_write;

    sync_proc : process(clock, reset)
    begin
        if (reset = '1') then
            current <= NULL_STATE;
        elsif (rising_edge(clock)) then
            current <= future;
        end if;
    end process sync_proc;

    comb_proc : process(all)
    begin
        future <= current;

        case (current.state) is
        end case;
    end process comb_proc;

end architecture;

