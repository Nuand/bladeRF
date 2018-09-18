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

entity bladerf_agc_lms_drv is
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

    gain_high       :  out  std_logic ;
    gain_mid        :  out  std_logic ;
    gain_low        :  out  std_logic ;

    -- Arbiter
    arbiter_req     :  out  std_logic ;
    arbiter_grant   :   in  std_logic ;
    arbiter_done    :  out  std_logic ;

    -- Misc
    band_sel        :   in  std_logic ;
        -- Physical Interface
    sclk            :   out std_logic ;
    miso            :   in  std_logic ;
    mosi            :   out std_logic ;
    cs_n            :   out std_logic

  ) ;
end entity ;

architecture arch of bladerf_agc_lms_drv is
    type gain_state_t is ( UNSET_GAIN_STATE, HIGH_GAIN_STATE, MID_GAIN_STATE, LOW_GAIN_STATE ) ;
    type lna_gain_t is ( LNA_MID_GAIN, LNA_MAX_GAIN ) ;
    type rxvga1_gain_t is ( RXVGA1_MID_GAIN, RXVGA1_MAX_GAIN ) ;
    type rxvga2_gain_t is ( RXVGA2_LOW_GAIN, RXVGA2_MID_GAIN ) ;

    type fsm_t is ( INIT, IDLE, SPI_WRITE, SPI_WRITE_GRANTED, WRITE_RXVGA1, WRITE_RXVGA2,
                    UPDATE_GAINS, SPI_WAIT, SPI_WAIT_1 ) ;

    type gain_t is record
        lna_gain            :   lna_gain_t ;
        rxvga1_gain         :   rxvga1_gain_t ;
        rxvga2_gain         :   rxvga2_gain_t ;
    end record ;

    -- -82dBm - -52dBm
    constant HIGH_GAIN : gain_t := (
            lna_gain     =>  LNA_MAX_GAIN,    --  6 dB (Max)
            rxvga1_gain  =>  RXVGA1_MAX_GAIN, -- 30 dB
            rxvga2_gain  =>  RXVGA2_MID_GAIN  -- 15 dB
        ) ;

    -- -52dBm - -30dBm
    constant MID_GAIN : gain_t := (
            lna_gain     =>  LNA_MID_GAIN,    --  3 dB (Mid)
            rxvga1_gain  =>  RXVGA1_MAX_GAIN, -- 30 dB
            rxvga2_gain  =>  RXVGA2_LOW_GAIN  --  0 dB
        ) ;

    -- -30dBm - -17dBm
    constant LOW_GAIN : gain_t := (
            lna_gain     =>  LNA_MID_GAIN,    --  3 dB (Mid)
            rxvga1_gain  =>  RXVGA1_MID_GAIN, -- 12 dB
            rxvga2_gain  =>  RXVGA2_LOW_GAIN  --  0 dB
        ) ;

    type state_t is record
        fsm                 :   fsm_t ;
        nfsm                :   fsm_t ;

        initializing        :   std_logic ;
        ack                 :   std_logic ;
        nack                :   std_logic ;
        gain_state          :   gain_state_t ;
        current_gain        :   gain_t ;
        future_gain         :   gain_t ;

        arbiter_req         :   std_logic ;
        arbiter_done        :   std_logic ;

        gain_inc_req        :   std_logic ;
        gain_dec_req        :   std_logic ;
        gain_rst_req        :   std_logic ;

        -- Avalon-MM Interface
        mm_write            :   std_logic ;
        mm_addr             :   std_logic_vector(7 downto 0) ;
        mm_din              :   std_logic_vector(7 downto 0) ;
    end record ;

    function NULL_STATE return state_t is
        variable rv : state_t ;
    begin
        rv.fsm          := INIT ;
        rv.nfsm         := INIT ;
        rv.initializing := '0' ;
        rv.arbiter_req  := '0' ;
        rv.arbiter_done := '0' ;
        rv.gain_inc_req := '0' ;
        rv.gain_dec_req := '0' ;
        rv.gain_rst_req := '0' ;
        rv.gain_state   := UNSET_GAIN_STATE ;
        rv.mm_addr      := ( others => '0' ) ;
        rv.mm_din       := ( others => '0' ) ;
        return rv ;
    end function ;

    signal current, future  :   state_t := NULL_STATE ;

    signal mm_busy          :   std_logic ;
begin

    arbiter_req  <= current.arbiter_req ;
    arbiter_done <= current.arbiter_done ;

    gain_high <= '1' when current.gain_state = HIGH_GAIN_STATE else '0' ;
    gain_mid  <= '1' when current.gain_state = MID_GAIN_STATE else '0' ;
    gain_low  <= '1' when current.gain_state = LOW_GAIN_STATE else '0' ;
    gain_ack <= current.ack ; --'1' when current.fsm = UPDATE_GAINS else '0' ;
    gain_nack <= current.nack ;

    U_spi_controller: entity work.lms6_spi_controller
      generic map (
        CLOCK_DIV       => 4
      )
      port map (
        -- Physical Interface
        sclk            => sclk,
        miso            => '0',
        mosi            => mosi,
        cs_n            => cs_n,

        -- Avalon-MM Interface
        mm_clock        => clock,
        mm_reset        => reset,
        mm_read         => '0',
        mm_write        => current.mm_write,
        mm_addr         => current.mm_addr,
        mm_din          => current.mm_din,
        mm_dout         => open,
        mm_dout_val     => open,                                  -- Read data valid.
        mm_busy         => mm_busy
    );

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
        future.mm_write <= '0' ;
        future.ack <= '0' ;
        future.nack <= '0' ;
        future.arbiter_done <= '0' ;
        future.arbiter_req  <= '0' ;

        future.gain_inc_req <= gain_inc_req ;
        future.gain_dec_req <= gain_dec_req ;
        future.gain_rst_req <= gain_rst_req ;

        case current.fsm is
            when INIT =>
                future.nack <= '1' ;
                if( enable = '1' ) then
                    future.nack <= '0' ;
                    future.gain_state <= HIGH_GAIN_STATE ;
                    future.future_gain <= HIGH_GAIN ;
                    future.initializing <= '1' ;
                    future.fsm <= SPI_WRITE ;
                end if ;

            when IDLE =>
                if( current.gain_inc_req = '1' ) then
                    if( current.gain_state = LOW_GAIN_STATE ) then
                        future.gain_state <= MID_GAIN_STATE ;
                        future.future_gain <= MID_GAIN ;
                        future.fsm <= SPI_WRITE ;
                    elsif( current.gain_state = MID_GAIN_STATE ) then
                        future.gain_state <= HIGH_GAIN_STATE ;
                        future.future_gain <= HIGH_GAIN ;
                        future.fsm <= SPI_WRITE ;
                    else
                        future.nack <= '1' ;
                        -- we are already as high as can be
                    end if ;
                end if ;
                if( current.gain_dec_req = '1' ) then
                    if( current.gain_state = MID_GAIN_STATE ) then
                        future.gain_state <= LOW_GAIN_STATE ;
                        future.future_gain <= LOW_GAIN ;
                        future.fsm <= SPI_WRITE ;
                    elsif( current.gain_state = HIGH_GAIN_STATE ) then
                        future.gain_state <= MID_GAIN_STATE ;
                        future.future_gain <= MID_GAIN ;
                        future.fsm <= SPI_WRITE ;
                    else
                        future.nack <= '1' ;
                        -- we are already as low as can be
                    end if ;
                end if ;
                if( current.gain_rst_req = '1' ) then
                    future.gain_state <= HIGH_GAIN_STATE ;
                    future.future_gain <= HIGH_GAIN ;
                end if ;
                if( enable = '0' ) then
                    future.fsm <= INIT ;
                end if ;

            when SPI_WRITE =>
                future.arbiter_req <= '1' ;
                if( arbiter_grant = '1' ) then
                    future.fsm <= SPI_WRITE_GRANTED ;
                end if ;

            when SPI_WRITE_GRANTED =>
                if( current.current_gain.lna_gain /= current.future_gain.lna_gain or
                      current.initializing = '1' ) then
                    future.mm_addr <= x"75" ;
                    if (current.future_gain.lna_gain = LNA_MAX_GAIN ) then
                        if (band_sel = '0' ) then
                            future.mm_din <= x"D0";
                        else
                            future.mm_din <= x"E0";
                        end if ;
                    elsif (current.future_gain.lna_gain = LNA_MID_GAIN ) then
                        if (band_sel = '0' ) then
                            future.mm_din <= x"90";
                        else
                            future.mm_din <= x"A0";
                        end if ;
                    end if ;
                    future.mm_write <= '1' ;
                    future.fsm <= SPI_WAIT ;
                    future.nfsm <= WRITE_RXVGA1 ;
                else
                    future.fsm <= WRITE_RXVGA1 ;
                end if ;

            when WRITE_RXVGA1 =>
                if( current.current_gain.rxvga1_gain /= current.future_gain.rxvga1_gain or
                      current.initializing = '1' ) then
                    future.mm_addr <= x"76" ;
                    if (current.future_gain.rxvga1_gain = RXVGA1_MAX_GAIN ) then
                        future.mm_din <= x"78" ;
                    elsif (current.future_gain.rxvga1_gain = RXVGA1_MID_GAIN ) then
                        future.mm_din <= x"46" ;
                    end if ;
                    future.mm_write <= '1' ;
                    future.fsm <= SPI_WAIT ;
                    future.nfsm <= WRITE_RXVGA2 ;
                else
                    future.fsm <= WRITE_RXVGA2 ;
                end if ;

            when WRITE_RXVGA2 =>
                if( current.current_gain.rxvga2_gain /= current.future_gain.rxvga2_gain or
                      current.initializing = '1' ) then
                    future.mm_addr <= x"65" ;
                    if (current.future_gain.rxvga2_gain = RXVGA2_MID_GAIN ) then
                        future.mm_din <= x"05" ;
                    elsif (current.future_gain.rxvga2_gain = RXVGA2_LOW_GAIN ) then
                        future.mm_din <= x"00" ;
                    end if ;
                    future.mm_write <= '1' ;
                    future.fsm <= SPI_WAIT ;
                    future.nfsm <= UPDATE_GAINS ;
                else
                    future.fsm <= UPDATE_GAINS ;
                end if ;

            when UPDATE_GAINS =>
                future.ack <= '1' ;
                future.initializing <= '0' ;
                future.current_gain <= current.future_gain ;
                future.arbiter_done <= '1' ;
                future.fsm <= IDLE ;

            when SPI_WAIT =>
                future.fsm <= SPI_WAIT_1 ;

            when SPI_WAIT_1 =>
                if( mm_busy = '0' ) then
                    future.fsm <= current.nfsm ;
                end if ;

        end case ;
    end process ;

end architecture ;
