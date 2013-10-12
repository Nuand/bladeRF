-- Copyright (c) 2013 Nuand LLC
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

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

entity fx3_model is
  port (
    fx3_pclk            :   out     std_logic ;
    fx3_gpif            :   inout   std_logic_vector(31 downto 0) ;
    fx3_ctl             :   inout   std_logic_vector(12 downto 0) ;
    fx3_uart_rxd        :   out     std_logic ;
    fx3_uart_txd        :   in      std_logic ;
    fx3_uart_csx        :   out     std_logic
  ) ;
end entity ; -- fx3_model

architecture digital_loopback of fx3_model is

    constant PCLK_HALF_PERIOD       : time  := 1 sec * (1.0/100.0e6/2.0) ;

    alias rxd_samples_request is fx3_ctl(0) ;
    alias rxd_samples_acknowledge is fx3_ctl(1) ;
    alias rxd_samples_we is fx3_ctl(2) ;
    alias txd_samples_we is fx3_ctl(3) ;
    alias gpif_reset is fx3_ctl(4) ;

    alias unused_ctl is fx3_ctl(12 downto 5) ;

    type gpif_state_t is (IDLE, TX_SAMPLES, RX_SAMPLES) ;
    signal gpif_state : gpif_state_t ;

    signal txd_samples : std_logic_vector(31 downto 0) ;

begin

    rxd_samples_request <= 'Z' ;
    rxd_samples_we <= 'Z' ;

    -- Create a 100MHz clock output
    fx3_pclk <= not fx3_pclk after PCLK_HALF_PERIOD when fx3_pclk = '0' or fx3_pclk = '1' else
                '1' ;

    -- GPIF interface
    fx3_gpif <= (others =>'Z') when rxd_samples_acknowledge = '1' else
            txd_samples ;

    stimulus : process
        constant BLOCK_SIZE : natural := 128 ;
        variable count : natural := 0 ;
    begin
        gpif_reset <= '1' ;
        gpif_state <= IDLE ;
        rxd_samples_acknowledge <= '0' ;
        txd_samples_we <= '0' ;
        txd_samples <= (others =>'0') ;
        for i in 0 to 10 loop
            wait until rising_edge( fx3_pclk ) ;
        end loop ;
        gpif_reset <= '0' ;
        while true loop
            wait until rising_edge( fx3_pclk ) ;
            case gpif_state is
                when IDLE =>
                    if( rxd_samples_request = '0' ) then
                        gpif_state <= TX_SAMPLES ;
                        txd_samples_we <= '1' ;
                    end if ;
                when TX_SAMPLES =>
                    txd_samples <= std_logic_vector(unsigned(txd_samples) + 1) ;
                    if( rxd_samples_request = '1' and count = 0) then
                        gpif_state <= RX_SAMPLES ;
                        txd_samples_we <= '0' ;
                        rxd_samples_acknowledge <= '1' ;
                        count := BLOCK_SIZE ;
                    elsif( count > 0 ) then
                        txd_samples_we <= '1' ;
                        count := count - 1 ;
                    end if ;
                when RX_SAMPLES =>
                    if( count = 1 ) then
                        gpif_state <= TX_SAMPLES ;
                        rxd_samples_acknowledge <= '0' ;
                        count := BLOCK_SIZE ;
                    else
                        count := count - 1 ;
                    end if ;
            end case ;
        end loop ;
    end process ;

    -- Unused control signals for now
    unused_ctl <= (others =>'0') ;

    -- TODO: UART Interface
    fx3_uart_rxd <= '0' ;
    fx3_uart_csx <= '1' ;

end architecture ; -- digital_loopback

architecture inband_scheduler of fx3_model is

begin

end architecture ; -- inband_scheduler
