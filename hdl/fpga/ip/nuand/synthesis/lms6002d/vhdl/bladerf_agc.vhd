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
    use ieee.math_real.all ;

entity bladerf_agc is
  port (
    -- 40MHz clock and async asserted, sync deasserted reset
    clock           :   in  std_logic ;
    reset           :   in  std_logic ;

    agc_hold_req    :   in  std_logic ;

    gain_inc_req    :  out  std_logic ;
    gain_dec_req    :  out  std_logic ;
    gain_rst_req    :  out  std_logic ;
    gain_ack        :   in  std_logic ;
    gain_nack       :   in  std_logic ;
    gain_max        :   in  std_logic ;

    rst_gains       :  out  std_logic ;
    burst           :  out  std_logic ;

    sample_i        :   in  signed(15 downto 0 ) ;
    sample_q        :   in  signed(15 downto 0 ) ;
    sample_valid    :   in  std_logic
  ) ;
end entity ;

architecture arch of bladerf_agc is

    type bladerf_sample_t is record
        i       :   signed(15 downto 0) ;
        q       :   signed(15 downto 0) ;
        valid   :   std_logic ;
    end record ;

    signal iir      :   signed( 31 downto 0 ) ;
    signal ptemp    :   signed( 31 downto 0 ) ;
    signal burst_cnt:   signed(  7 downto 0 ) ;

    function run_iir( x : signed( 31 downto 0); y : signed ( 31 downto 0) )
    return signed
    is
        variable amrea : signed(31 downto 0) ;
    begin
        amrea := resize( x - shift_right(x, 6) + shift_right(y, 6), 32 );
        return amrea;
    end;

    type fsm_t is (IDLE, SETTLE, ATTACK, WAIT_GAIN_ACK, WAIT_GAIN_ACK_1, HOLD) ;

    type state_t is record
        fsm                 :   fsm_t ;
        inc_req             :   std_logic ;
        dec_req             :   std_logic ;
        rst_req             :   std_logic ;
        timer               :   unsigned( 10 downto 0 ) ;
    end record ;

    function NULL_STATE return state_t is
        variable rv : state_t ;
    begin
        rv.fsm          := IDLE ;
        rv.inc_req      := '0' ;
        rv.dec_req      := '0' ;
        rv.rst_req      := '0' ;
        rv.timer        := ( others => '0' );
        return rv ;
    end function ;

    signal current, future  :   state_t := NULL_STATE ;
    signal sample, sample_out : bladerf_sample_t ;
begin
    sample.i <= sample_i;
    sample.q <= sample_q;
    sample.valid <= sample_valid;

    gain_inc_req <= current.inc_req ;
    gain_dec_req <= current.dec_req ;
    gain_rst_req <= current.rst_req ;

    process( clock, reset )
    begin
        if( reset = '1' ) then
            rst_gains <= '0' ;
            burst <= '0' ;
        elsif( rising_edge( clock )) then
            if( ( gain_max = '1' and iir < 1200 ) or current.fsm = SETTLE ) then
                rst_gains <= '1' ;
            else
                rst_gains <= '0' ;
            end if ;

            if( gain_max = '0' or iir > 1100 ) then
                burst <= '1' ;
                burst_cnt <= to_signed(18, burst_cnt'length) ;
            else
                if( ptemp > 1100 ) then
                    burst <= '1' ;
                    burst_cnt <= to_signed(6, burst_cnt'length) ;
                else
                    if( burst_cnt > 0 ) then
                        burst <= '1' ;
                        burst_cnt <= burst_cnt - 1 ;
                    else
                        burst <= '0' ;
                    end if ;
                end if ;
            end if ;
        end if ;
    end process ;

    process( clock, reset )
    begin
        if( reset = '1' ) then
            iir <= ( others => '0' ) ;
            ptemp <= (others => '0' ) ;
        elsif( rising_edge( clock )) then
            if( current.timer = 18 ) then
                iir <= ptemp ;
            elsif( sample.valid = '1') then
                ptemp <= sample.i * sample.i + sample.q * sample.q ;
                iir <= run_iir(iir, ptemp) ;
            end if ;
        end if ;
    end process ;

    sync : process(clock, reset)
    begin
        if( reset = '1' ) then
            current <= NULL_STATE ;
        elsif( rising_edge(clock) ) then
            current <= future ;
        end if ;
    end process ;

    comb : process(all)
    begin
        future <= current ;
        future.inc_req <= '0' ;
        future.dec_req <= '0' ;
        future.rst_req <= '0' ;

        case current.fsm is
            when IDLE =>
                future.fsm <= SETTLE ;

            when SETTLE =>
                if( current.timer > 32 ) then -- 50 looks good 70 is overkill
                    future.timer <= ( others => '0' ) ;
                    future.fsm <= ATTACK ;
                else
                    if( current.timer < 400 ) then
                        future.timer <= current.timer + 1 ;
                    end if ;
                end if ;

            when ATTACK =>
                if( rst_gains = '1' ) then
                    future.timer <= ( others => '0' ) ;
                else
                    if( current.timer < 400 ) then
                        future.timer <= current.timer + 1 ;
                    end if ;
                end if ;

                if( agc_hold_req = '1' ) then
                    future.fsm <= HOLD ;
                else
                    -- 405000, -48dBm to enter mid, -28dBm to enter low
                    if( current.timer > 24 and iir > 335000 ) then --335000
                        future.fsm <= WAIT_GAIN_ACK ;
                        future.dec_req <= '1' ;
                    elsif( current.timer < 24 and iir > 485000 ) then --335000
                        -- I = Q = sqrt(2048*2048/10)=657, IIR = I^2 + Q^2= 845000
                        future.fsm <= WAIT_GAIN_ACK ;
                        future.dec_req <= '1' ;
                    elsif( current.timer > 300 and iir < 1000 ) then
                    --elsif( iir < 5000 ) then
                        -- I = Q = 50, IIR = I^2 + Q^2= 5000
                        future.inc_req <= '1' ;
                        future.fsm <= WAIT_GAIN_ACK ;
                    end if ;
                end if ;

            when WAIT_GAIN_ACK =>
                future.fsm <= WAIT_GAIN_ACK_1 ;
            when WAIT_GAIN_ACK_1 =>
                if( gain_ack = '1' ) then
                    future.fsm <= SETTLE ;
                    future.timer <= ( others => '0' ) ;
                end if ;
                if( gain_nack = '1' ) then
                    future.fsm <= ATTACK ;
                end if ;

            when HOLD =>
                if( iir < 5000 ) then
                    -- I = Q = 50, IIR = I^2 + Q^2= 5000
                    future.rst_req <= '1' ;
                    future.fsm <= WAIT_GAIN_ACK ;
                end if ;

        end case ;
    end process ;

end architecture ;
