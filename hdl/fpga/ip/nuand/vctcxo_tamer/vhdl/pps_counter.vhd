-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

-- -----------------------------------------------------------------------------
-- Entity:      pps_counter
-- Standard:    VHDL-2008
-- Description: Counts number of VCTCXO clock cycles that have occurred between
--              the specified number of 1PPS pulses. The count is output
--              in the system clock domain along with a valid signal.
-- -----------------------------------------------------------------------------
entity pps_counter is
    generic(
        COUNT_WIDTH     : positive := 64;
        PPS_PULSES      : positive range 1 to 1023
    );
    port(
        -- System-synchronous signals
        clock           : in  std_logic;
        reset           : in  std_logic;
        count           : out signed(COUNT_WIDTH-1 downto 0);
        count_strobe    : out std_logic;

        -- Measurement signals
        vctcxo_clock    : in  std_logic;
        pps             : in  std_logic
    );
end entity;

architecture arch of pps_counter is

    -- FSM States
    type fsm_t is (
        IDLE,
        INCREMENT,
        HANDSHAKE_REQ
    );

    -- State of internal signals
    type state_t is record
        state           : fsm_t;
        pps_count       : natural range 0 to 1023;
        handshake_d     : signed(COUNT_WIDTH-1 downto 0);
        handshake_dv    : std_logic;
    end record;

    constant reset_value : state_t := (
        state           => IDLE,
        pps_count       => 0,
        handshake_d     => (others => '-'),
        handshake_dv    => '0'
    );

    signal current            : state_t;
    signal future             : state_t;

    signal counter_reset      : std_logic := '1';
    signal handshake_ack      : std_logic := '0';
    signal count_strobe_pulse : std_logic := '0';

begin

    -- Counter reset control process. When the counter is reset, it may have
    -- been because of a trim DAC retune. Need to keep the counter FSM in reset
    -- for a period of time to allow the trim DAC to settle into its new value.
    -- A retune takes about 800 us for a full swing transition. Using the VCTCXO
    -- clock here because it's slower (easier to meet timing, fewer counter
    -- bits), and the change in precision during a retune doesn't matter here.
    reset_proc : process( vctcxo_clock, reset )
        -- Number of VCTCXO clock cycles before releasing the counter reset.
        -- A 15-bit counter starting at 0x7FFF will take ~850 us at 38.4 MHz.
        variable rst_cnt_v : unsigned(14 downto 0) := (others => '1');
    begin
        if( reset = '1' ) then
            rst_cnt_v     := (others => '1');
            counter_reset <= '1';
        elsif( rising_edge( vctcxo_clock ) ) then
            if( rst_cnt_v /= 0 ) then
                rst_cnt_v := rst_cnt_v - 1;
                counter_reset <= '1';
            else
                counter_reset <= '0';
            end if;
        end if;
    end process;

    sync_proc : process( vctcxo_clock, counter_reset )
    begin
        if( counter_reset = '1' ) then
            current <= reset_value;
        elsif( rising_edge(vctcxo_clock) ) then
            current <= future;
        end if;
    end process;

    comb_proc : process( all )
    begin

        -- Next state defaults
        future <= current;
        future.handshake_dv <= '0';

        case( current.state ) is
            when IDLE =>
                future.handshake_d <= (others => '0');
                future.pps_count   <= 0;
                if( pps = '1' ) then
                    future.state <= INCREMENT;
                end if;
            when INCREMENT =>
                if( pps = '1' ) then
                    future.pps_count <= current.pps_count + 1;
                end if;
                if( current.pps_count < PPS_PULSES ) then
                    future.handshake_d <= current.handshake_d + 1;
                else
                    future.handshake_dv <= '1';
                    future.state        <= HANDSHAKE_REQ;
                end if;
            when HANDSHAKE_REQ =>
                future.handshake_dv <= '1';
                if( handshake_ack = '1' ) then
                    future.handshake_dv <= '0';
                    future.state        <= IDLE;
                end if;
        end case;
    end process;

    -- Use handshaking to get the clock count value into the system clock domain
    U_handshake : entity work.handshake
        generic map (
            DATA_WIDTH          =>  COUNT_WIDTH
        ) port map (
            source_reset        =>  counter_reset,
            source_clock        =>  vctcxo_clock,
            source_data         =>  std_logic_vector(current.handshake_d),

            dest_reset          =>  reset,
            dest_clock          =>  clock,
            signed(dest_data)   =>  count,
            dest_req            =>  current.handshake_dv,
            dest_ack            =>  handshake_ack
        );

    -- Generate a single-cycle 'count valid' pulse
    U_pulse_gen_count_strobe : entity work.pulse_gen
        generic map (
            EDGE_RISE       => '1',
            EDGE_FALL       => '0'
        )
        port map (
            clock           => clock,
            reset           => reset,
            sync_in         => handshake_ack,
            pulse_out       => count_strobe
        );

end architecture;
