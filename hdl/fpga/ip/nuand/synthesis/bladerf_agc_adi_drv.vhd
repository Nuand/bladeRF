-- Copyright (c) 2020 Nuand LLC
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

entity bladerf_agc_adi_drv is
  port (
    -- 80 MHz clock and async asserted, sync deasserted reset
    clock           :   in  std_logic ;
    reset           :   in  std_logic ;

    enable          :   in  std_logic ;
    gain_inc_req    :   in  std_logic ;
    gain_dec_req    :   in  std_logic ;
    gain_rst_req    :   in  std_logic ;
    gain_ack        :  out  std_logic ;
    gain_nack       :  out  std_logic ;

    gain_max        :  out  std_logic ;

    -- Physical Interface
    ctrl_gain_inc   :   out std_logic ;
    ctrl_gain_dec   :   out std_logic

  ) ;
end entity ;

architecture arch of bladerf_agc_adi_drv is
    type gain_state_t is ( UNSET_GAIN_STATE, HIGH_GAIN_STATE, MID_GAIN_STATE, LOW_GAIN_STATE ) ;

    type fsm_t is ( INIT, IDLE, CTRL_WRITE_SETUP, CTRL_WRITE, UPDATE_GAINS, CTRL_WAIT ) ;


    -- High gain, total gain: 51dB
    -- -82dBm - -52dBm

    -- Mid gain, total gain: 33dB
    -- -52dBm - -30dBm

    -- Low gain, total gain: 15dB
    -- -30dBm - -17dBm

    type state_t is record
        fsm                 :   fsm_t ;
        nfsm                :   fsm_t ;

        initializing        :   std_logic ;
        ack                 :   std_logic ;
        nack                :   std_logic ;
        gain_state          :   gain_state_t ;

        gain_inc_req        :   std_logic ;
        gain_dec_req        :   std_logic ;
        gain_rst_req        :   std_logic ;

        gain_dir_inc        :   std_logic ;
        gain_dir_num        :   natural range 0 to 10 ;

        timeout_cnt         :   natural range 0 to 10 ;

        ctrl_gain_inc       :   std_logic ;
        ctrl_gain_dec       :   std_logic ;

    end record ;

    function NULL_STATE return state_t is
        variable rv : state_t ;
    begin
        rv.fsm           := INIT ;
        rv.nfsm          := INIT ;
        rv.initializing  := '0' ;

        rv.gain_inc_req  := '0' ;
        rv.gain_dec_req  := '0' ;
        rv.gain_rst_req  := '0' ;
        rv.gain_state    := UNSET_GAIN_STATE ;

        rv.gain_dir_inc  := '0';
        rv.gain_dir_num  := 0;

        rv.timeout_cnt   := 0;
        rv.ctrl_gain_inc := '0';
        rv.ctrl_gain_dec := '0';
        return rv ;
    end function ;

    signal current, future  :   state_t := NULL_STATE ;

begin

    gain_max  <= '1' when current.gain_state = HIGH_GAIN_STATE else '0' ;
    gain_ack  <= current.ack ; --'1' when current.fsm = UPDATE_GAINS else '0' ;
    gain_nack <= current.nack ;

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
        future.ack <= '0' ;
        future.nack <= '0' ;

        future.gain_inc_req <= gain_inc_req ;
        future.gain_dec_req <= gain_dec_req ;
        future.gain_rst_req <= gain_rst_req ;

        future.ctrl_gain_inc <= '0' ;
        future.ctrl_gain_dec <= '0' ;

        case current.fsm is
            when INIT =>
                future.nack <= '1' ;
                if( enable = '1' ) then
                    future.nack <= '0' ;
                    future.gain_dir_inc <= '0' ;
                    future.gain_dir_num <= 3 ;

                    future.gain_state <= HIGH_GAIN_STATE ;
                    future.initializing <= '1' ;

                    future.fsm <= CTRL_WRITE_SETUP ;
                end if ;

            when IDLE =>
                if( current.gain_inc_req = '1' ) then
                    future.gain_dir_inc <= '1' ;
                    if( current.gain_state = LOW_GAIN_STATE ) then
                        future.gain_state <= MID_GAIN_STATE ;
                        future.gain_dir_num <= 1;
                        future.fsm <= CTRL_WRITE_SETUP ;
                    elsif( current.gain_state = MID_GAIN_STATE ) then
                        future.gain_state <= HIGH_GAIN_STATE ;
                        future.gain_dir_num <= 1;
                        future.fsm <= CTRL_WRITE_SETUP ;
                    else
                        future.nack <= '1' ;
                        -- gain is already maxed out
                    end if ;
                end if ;
                if( current.gain_dec_req = '1' ) then
                    future.gain_dir_inc <= '0' ;
                    if( current.gain_state = MID_GAIN_STATE ) then
                        future.gain_state <= LOW_GAIN_STATE ;
                        future.gain_dir_num <= 1;
                        future.fsm <= CTRL_WRITE_SETUP ;
                    elsif( current.gain_state = HIGH_GAIN_STATE ) then
                        future.gain_state <= MID_GAIN_STATE ;
                        future.gain_dir_num <= 1;
                        future.fsm <= CTRL_WRITE_SETUP ;
                    else
                        future.nack <= '1' ;
                        -- gain is already at absolute minimum
                    end if ;
                end if ;
                if( current.gain_rst_req = '1' ) then
                    future.fsm <= INIT ;
                end if ;
                if( enable = '0' ) then
                    future.fsm <= INIT ;
                end if ;

            when CTRL_WRITE_SETUP =>
                future.timeout_cnt <= 0 ;
                future.fsm <= CTRL_WRITE ;

            when CTRL_WRITE =>
                if( current.gain_dir_inc = '1' ) then
                    future.ctrl_gain_inc <= '1' ;
                else
                    future.ctrl_gain_dec <= '1' ;
                end if;

                future.timeout_cnt <= current.timeout_cnt + 1;
                if( current.timeout_cnt > 4 ) then
                    future.gain_dir_num <= current.gain_dir_num - 1;
                    future.fsm <= CTRL_WAIT ;
                end if ;

            when CTRL_WAIT =>
                future.timeout_cnt <= current.timeout_cnt + 1;
                if( current.timeout_cnt > 4 ) then
                    if( current.gain_dir_num = 0 ) then
                        future.fsm <= UPDATE_GAINS ;
                    else
                        future.fsm <= CTRL_WRITE_SETUP ;
                    end if;
                end if;

            when UPDATE_GAINS =>
                future.ack <= '1' ;
                future.initializing <= '0' ;
                future.fsm <= IDLE ;

        end case ;
    end process ;

end architecture ;
