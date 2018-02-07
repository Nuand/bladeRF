-- Copyright (c) 2017 Nuand LLC
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Affero General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Affero General Public License for more details.
--
-- You should have received a copy of the GNU Affero General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity arbiter is
  generic (
    N : integer := 1
  ) ;

  port (
    -- Control signals
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;

    -- Memory mapped interface
    addr        :   in  std_logic_vector(4 downto 0) ;
    din         :   in  std_logic_vector(7 downto 0) ;
    dout        :   out std_logic_vector(7 downto 0) ;
    write       :   in  std_logic ;
    read        :   in  std_logic ;
    waitreq     :   out std_logic ;
    readack     :   out std_logic ;
    intr        :   out std_logic ;

    -- Arbiter signals
    request     :   in  std_logic_vector(N-1 downto 0) ;
    ack         :   in  std_logic_vector(N-1 downto 0) ;

    granted     :   out std_logic_vector(N-1 downto 0)
  ) ;
end entity ;

architecture arch of arbiter is

    signal rack     : std_logic ;

    signal uaddr    :   unsigned(addr'range) ;

    signal intr_clear   :   std_logic ;
    signal intr_set     :   std_logic ;

    signal nios_csr     :   std_logic_vector(1 downto 0) ;
    signal nios_request :   std_logic ;
    signal nios_ack     :   std_logic ;
    signal nios_granted :   std_logic ;

    signal arbiter_request :  std_logic_vector(N-1 downto 0) ;
    signal arbiter_ack     :  std_logic_vector(N-1 downto 0) ;
    signal arbiter_granted :  std_logic_vector(N-1 downto 0) ;

    type fsm_t is (INIT, WAIT_FOR_REQ, GRANT, WAIT_FOR_ACK) ;

    type state_t is record
        state           : fsm_t;
        grant           : std_logic_vector(N-1 downto 0) ;
        mask            : std_logic_vector(N-1 downto 0) ;
        mask_valid      : std_logic ;
    end record;

    function NULL_STATE return state_t is
        variable rv : state_t ;
    begin
        rv.state := INIT ;
        rv.grant      := ( others => '0' ) ;
        rv.mask       := ( others => '1' ) ;
        rv.mask_valid := '0' ;

        return rv ;
    end function;


    signal current : state_t ;
    signal future  : state_t ;

begin

    nios_request <= nios_csr(0) ;
    nios_ack <= nios_csr(1) ;
    nios_granted <= current.grant(0) ;

    waitreq <= '0';
    readack <= rack ;

    uaddr <= unsigned(addr) ;

    generate_ack : process(clock, reset)
    begin
        if( reset = '1' ) then
            rack <= '0' ;
        elsif( rising_edge(clock) ) then
            if( read = '1' ) then
                rack <= '1' ;
            else
                rack <= '0' ;
            end if ;
        end if ;
    end process ;

    mm_read : process(clock)
    begin
        if( rising_edge(clock) ) then
            case to_integer(uaddr) is
                when 0  => dout <= "000000"  & nios_csr ;
                when 1  => dout <= "0000000" & nios_granted ;

                when others  => dout <= x"13";
            end case;
        end if ;
    end process;

    mm_write : process(clock, reset)
    begin
        if( reset = '1' ) then
            nios_csr <= ( others => '0' ) ;
        elsif( rising_edge(clock) ) then
            intr_set <= '0' ;
            intr_clear <= '0' ;
            if( write = '1' ) then
                case to_integer(uaddr) is
                    when 0 => nios_csr <= din(nios_csr'range) ;

                    when 8 =>
                        -- Command coming in!
                        case to_integer(unsigned(din)) is
                            when 0 =>
                                -- Set interrupt
                                intr_set <= '1' ;

                            when 1 =>
                                -- Clear interrupt
                                intr_clear <= '1' ;

                            when others =>
                                null ;

                        end case ;

                    when others => null ;
                end case ;
            end if ;
        end if ;
    end process ;

    arbiter_request <= request(N-1 downto 1) & nios_request ;
    arbiter_ack <= ack(N-1 downto 1) & nios_ack ;
    granted <= current.grant ;

    sync_proc : process(clock, reset)
    begin
        if( reset = '1' ) then
            current <= NULL_STATE ;
        elsif( rising_edge(clock) ) then
            current <= future;
        end if;
    end process;

    comb_proc : process( all )
        variable arbiter_request_bit : std_logic_vector(N-1 downto 0) ;
    begin

        -- Next state defaults
        future <= current;

        case( current.state ) is

            when INIT =>
                future.state <= WAIT_FOR_REQ ;

            when WAIT_FOR_REQ =>
                if( arbiter_request /= ( arbiter_request'range => '0' ) ) then
                    arbiter_request_bit := std_logic_vector(unsigned(not(arbiter_request)) + 1 ) ;
                    if( (arbiter_request and current.mask) = ( arbiter_request'range => '0' ) ) then
                        future.grant <= arbiter_request and arbiter_request_bit ;
                    else
                        future.grant <= (arbiter_request and current.mask) and arbiter_request_bit ;
                    end if ;
                    future.state <= GRANT ;
                end if ;

            when GRANT =>
                -- it's fine if this goes to 0, the unmasked check gives requester #0 the highest priority
                future.mask <= std_logic_vector(unsigned(current.grant) - 1 ) ;
                future.state <= WAIT_FOR_ACK ;

            when WAIT_FOR_ACK =>
                --if( arbiter_ack /= ( arbiter_ack'range => '0' ) ) then
                if( (arbiter_ack and current.grant) = current.grant ) then
                    future.grant <= ( others => '0' ) ;
                    future.state <= WAIT_FOR_REQ ;
                end if ;

        end case ;
    end process ;

    interrupt : process(clock, reset)
        type intrfsm_t is (WAIT_FOR_ARM, WAIT_FOR_GRANT, WAIT_FOR_CLEAR) ;
        variable intrfsm : intrfsm_t := WAIT_FOR_ARM ;
    begin
        if( reset = '1' ) then
            intr <= '0' ;
            intrfsm := WAIT_FOR_ARM ;
        elsif( rising_edge(clock) ) then
            case intrfsm is
                when WAIT_FOR_ARM =>
                    if( intr_set = '1' ) then
                        intrfsm := WAIT_FOR_GRANT ;
                    end if ;
                when WAIT_FOR_GRANT =>
                    if( nios_granted = '1' ) then
                        intr <= '1' ;
                        intrfsm := WAIT_FOR_CLEAR ;
                    end if ;
                when WAIT_FOR_CLEAR =>
                    if( intr_clear = '1' ) then
                        intr <= '0' ;
                        intrfsm := WAIT_FOR_ARM ;
                    end if ;
            end case ;
        end if ;
    end process ;
end architecture ;

