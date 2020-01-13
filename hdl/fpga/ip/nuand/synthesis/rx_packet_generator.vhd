-- Copyright (c) 2020 Nuand LLC
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
    use ieee.math_real.all;
    use ieee.math_complex.all;

library work;
    use work.fifo_readwrite_p.all;


entity rx_packet_generator is
    generic (
        PACKET_LEN         : integer := 500;
        MAX_LEN            : integer := 1000;
        INCR               : integer := 50;
        GAP                : integer := 10
    );

    port (
        rx_reset               : in    std_logic;
        rx_clock               : in    std_logic;
        rx_enable              : in    std_logic;
        rx_packet_enable       : in    std_logic;

        rx_packet_ready        : in    std_logic;

        rx_packet_control      : out   packet_control_t
    );
end entity;

architecture arch of rx_packet_generator is
    type state_t is (
        IDLE,
        HOLDOFF,
        WAITED,
        WRITE
    );

    type fsm_t is record
        state       : state_t;
        hold_count  : integer range -1 to GAP + 2;
        write_count : integer range -1 to PACKET_LEN;
        pkt_id      : integer range 0 to 65536;
        pkt         : packet_control_t;
    end record;

    constant FSM_RESET_VALUE : fsm_t := (
        state       => IDLE,
        hold_count  => 0,
        write_count => 0,
        pkt_id      => 0,
        pkt         => PACKET_CONTROL_DEFAULT
    );

    signal current : fsm_t := FSM_RESET_VALUE;
    signal future  : fsm_t := FSM_RESET_VALUE;

begin

    fsm_proc : process (rx_clock, rx_reset)
    begin
        if( rx_reset = '1' ) then
            current <= FSM_RESET_VALUE;
        elsif( rising_edge( rx_clock ) ) then
            current <= future;
        end if;
    end process;

    rx_packet_control <= current.pkt;

    fsm_comb : process(all)
    begin

        future <= current;
        future.pkt <= PACKET_CONTROL_DEFAULT;

        case current.state is
            when IDLE =>

                future.hold_count <= 0;
                future.write_count <= 0;
                if( rx_enable = '1' and rx_packet_enable = '1' ) then
                    future.state <= HOLDOFF;
                end if;

            when HOLDOFF =>
                future.write_count <= PACKET_LEN;
                future.hold_count <= current.hold_count + 1;
                if(current.hold_count = GAP) then
                    future.state <= WAITED;
                end if;

            when WAITED =>
                if( rx_packet_ready = '1' ) then
                    future.state <= WRITE;
                end if;

            when WRITE =>

                future.pkt.data_valid <= '1';
                future.pkt.data <= std_logic_vector(to_unsigned(current.pkt_id, 16)) & std_logic_vector(to_unsigned(current.write_count, 16));

                if(current.write_count = PACKET_LEN) then
                    future.pkt.pkt_sop <= '1';
                    future.pkt.data_valid <= '1';
                elsif(current.write_count = 1) then
                    future.pkt.pkt_eop <= '1';
                    future.state <= IDLE;
                    if( current.pkt_id > 65000) then
                        future.pkt_id <= 0;
                    else
                        future.pkt_id <= current.pkt_id + 1;
                    end if;
                end if;

                future.write_count <= current.write_count - 1;

            when others =>

                future <= FSM_RESET_VALUE;
        end case;

    end process;
end architecture;
