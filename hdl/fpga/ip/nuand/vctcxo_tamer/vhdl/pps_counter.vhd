-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

-- Copyright (c) 2015 Nuand LLC
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

-- -----------------------------------------------------------------------------
-- Entity:      pps_counter
-- Standard:    VHDL-2008
-- Description: Counts number of VCTCXO clock cycles that have occurred between
--              the specified number of 1PPS pulses. The count is output
--              in the system clock domain along with a valid signal.
-- -----------------------------------------------------------------------------
entity pps_counter is
    generic(
        COUNT_WIDTH  : positive := 64;
        PPS_PULSES   : positive range 1 to 1023
    );
    port(
        -- System-synchronous signals
        sys_clock    : in  std_logic;
        sys_reset    : in  std_logic;
        sys_count    : out signed(COUNT_WIDTH-1 downto 0);
        sys_count_v  : out std_logic;

        -- Measurement signals
        vctcxo_clock : in  std_logic;
        vctcxo_reset : in  std_logic;
        vctcxo_pps   : in  std_logic
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

    -- System clock domain
    signal sys_hs_req         : std_logic := '0';
    signal sys_hs_ack         : std_logic := '0';

    -- VCTCXO clock domain
    signal vctcxo_reset_hold  : std_logic := '1';
    signal vctcxo_hs_ack      : std_logic := '0';

begin

    -- Counter reset control process. When the counter is reset, it may have
    -- been because of a trim DAC retune. Need to keep the counter FSM in reset
    -- for a period of time to allow the trim DAC to settle into its new value.
    -- A retune takes about 800 us for a full swing transition. Using the VCTCXO
    -- clock here because it's slower (easier to meet timing, fewer counter
    -- bits), and the change in precision during a retune doesn't matter here.
    reset_proc : process( vctcxo_clock, vctcxo_reset )
        -- Number of VCTCXO clock cycles before releasing the counter reset.
        -- A 15-bit counter starting at 0x7FFF will take ~850 us at 38.4 MHz.
        variable rst_cnt_v : unsigned(14 downto 0) := (others => '1');
    begin
        if( vctcxo_reset = '1' ) then
            rst_cnt_v := (others => '1');
            vctcxo_reset_hold <= '1';
        elsif( rising_edge( vctcxo_clock ) ) then
            if( rst_cnt_v /= 0 ) then
                rst_cnt_v := rst_cnt_v - 1;
                vctcxo_reset_hold <= '1';
            else
                vctcxo_reset_hold <= '0';
            end if;
        end if;
    end process;

    sync_proc : process( vctcxo_clock, vctcxo_reset_hold )
    begin
        if( vctcxo_reset_hold = '1' ) then
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
                if( vctcxo_pps = '1' ) then
                    future.state <= INCREMENT;
                end if;
            when INCREMENT =>
                if( vctcxo_pps = '1' ) then
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
                if( vctcxo_hs_ack = '1' ) then
                    future.handshake_dv <= '0';
                    future.state        <= IDLE;
                end if;
        end case;
    end process;

    -- Bring the handshake ack into VCTCXO clock domain
    U_sync_hs_ack : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        ) port map (
            reset       =>  vctcxo_reset,
            clock       =>  vctcxo_clock,
            async       =>  sys_hs_ack,
            sync        =>  vctcxo_hs_ack
        );

    -- Bring the handshake req into System clock domain
    U_sync_hs_req : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        ) port map (
            reset       =>  sys_reset,
            clock       =>  sys_clock,
            async       =>  current.handshake_dv,
            sync        =>  sys_hs_req
        );

    -- Use handshaking to get the clock count value into the system clock domain
    U_handshake : entity work.handshake
        generic map (
            DATA_WIDTH          =>  COUNT_WIDTH
        ) port map (
            source_reset        =>  vctcxo_reset_hold,
            source_clock        =>  vctcxo_clock,
            source_data         =>  std_logic_vector(current.handshake_d),
            dest_reset          =>  sys_reset,
            dest_clock          =>  sys_clock,
            signed(dest_data)   =>  sys_count,
            dest_req            =>  sys_hs_req,
            dest_ack            =>  sys_hs_ack
        );

    -- Generate a single-cycle 'count valid' pulse
    U_edge_detector_count_v : entity work.edge_detector
        generic map (
            EDGE_RISE       => '1',
            EDGE_FALL       => '0'
        )
        port map (
            clock           => sys_clock,
            reset           => sys_reset,
            sync_in         => sys_hs_ack,
            pulse_out       => sys_count_v
        );

end architecture;
