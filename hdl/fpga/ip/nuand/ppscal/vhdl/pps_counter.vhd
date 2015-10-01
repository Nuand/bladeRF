-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

-- -----------------------------------------------------------------------------
-- Entity:      pps_counter
-- Description: Counts number of TCXO clock cycles that have occurred between
--              the specified number of 1PPS pulses. The count is output
--              in the system clock domain along with a valid signal.
-- Standard:    VHDL-2008
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
        tcxo_clock      : in  std_logic;
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

    signal handshake_ack      : std_logic := '0';
    signal count_strobe_pulse : std_logic := '0';

begin

    sync_proc : process( tcxo_clock, reset )
    begin
        if( reset = '1' ) then
            current <= reset_value;
        elsif( rising_edge(tcxo_clock) ) then
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
            source_reset        =>  reset,
            source_clock        =>  tcxo_clock,
            source_data         =>  std_logic_vector(current.handshake_d),

            dest_reset          =>  reset,
            dest_clock          =>  clock,
            signed(dest_data)   =>  count,
            dest_req            =>  current.handshake_dv,
            dest_ack            =>  handshake_ack
        );


    count_strobe_proc : process( clock, reset )
        variable armed : boolean := true;
    begin
        if( reset = '1' ) then
            count_strobe_pulse <= '0';
            armed := true;
        elsif( rising_edge(clock) ) then
            count_strobe_pulse <= '0';
            if( armed = true ) then
                if( handshake_ack = '1' ) then
                    count_strobe_pulse <= '1';
                    armed := false;
                end if;
            else
                if( handshake_ack = '0' ) then
                    armed := true;
                end if;
            end if;
        end if;
    end process;

    -- Output to application
    count_strobe <= count_strobe_pulse;

end architecture;
