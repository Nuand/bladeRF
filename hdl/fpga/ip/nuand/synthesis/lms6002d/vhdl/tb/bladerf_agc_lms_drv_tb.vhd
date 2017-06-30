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

entity bladerf_agc_lms_drv_tb is
end entity ;

architecture arch of bladerf_agc_lms_drv_tb is

    signal  clock           :       std_logic := '0' ;
    signal  reset           :       std_logic ;

    signal  enable          :       std_logic ;
    signal  gain_inc_req    :       std_logic ;
    signal  gain_dec_req    :       std_logic ;
    signal  gain_rst_req    :       std_logic ;
    signal  gain_ack        :       std_logic ;
    signal  gain_nack       :       std_logic ;

    signal  gain_high       :       std_logic ;
    signal  gain_mid        :       std_logic ;
    signal  gain_low        :       std_logic ;

    signal  arbiter_req     :       std_logic ;
    signal  arbiter_grant   :       std_logic ;
    signal  arbiter_done    :       std_logic ;

    signal  band_sel        :       std_logic ;

    signal  sclk            :       std_logic ;
    signal  miso            :       std_logic ;
    signal  mosi            :       std_logic ;
    signal  cs_n            :       std_logic ;

begin

    U_agc_tb : entity work.bladerf_agc_lms_drv
       port map(
              clock          => clock,
              reset          => reset,
              
              enable         => enable,
              gain_inc_req   => gain_inc_req,
              gain_dec_req   => gain_dec_req,
              gain_rst_req   => gain_rst_req,
              gain_ack       => gain_ack,
              gain_nack      => gain_nack,
              
              gain_high      => gain_high,
              gain_mid       => gain_mid,
              gain_low       => gain_low,
              
              arbiter_req    => arbiter_req,
              arbiter_grant  => arbiter_grant,
              arbiter_done   => arbiter_done,

              band_sel       => band_sel,

              sclk           => sclk,
              miso           => miso,
              mosi           => mosi,
              cs_n           => cs_n
       );

    clock  <= not clock after 10 ns;
    reset  <= '1', '0' after 50 ns;
    enable <= '0' , '1' after 100 ns;

    band_sel     <= '1';

    gain_inc_req <= '0';
    gain_dec_req <= '0';
    gain_rst_req <= '0';

    arbiter_grant <= '0', '1' after 170 ns;

end architecture ;
