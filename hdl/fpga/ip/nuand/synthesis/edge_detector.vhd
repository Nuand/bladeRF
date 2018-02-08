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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity edge_detector is
   generic (
      EDGE_RISE  :  std_logic ;
      EDGE_FALL  :  std_logic ;
      NUM_PULSES :  positive range 1 to 1023 := 2
   ) ;
   port (
      clock      :  in std_logic ;
      reset      :  in std_logic ;
      sync_in    :  in std_logic ;
      pulse_out  : out std_logic
   ) ;
end entity ;

architecture arch of edge_detector is
   signal pulses : std_logic_vector( NUM_PULSES - 1 downto 0 ) ;
begin

   process( clock, reset )
   begin
      if( reset = '1' ) then
         pulses <= ( others => '0' ) ;
         pulse_out <= '0' ;
      elsif( rising_edge( clock ) ) then
         pulses <= sync_in & pulses( NUM_PULSES - 1 downto 1 ) ;

         if( EDGE_RISE = '1' and pulses(1 downto 0) = "10" ) then
            pulse_out <= '1' ;
         elsif ( EDGE_FALL = '1' and pulses(1 downto 0) = "01" ) then
            pulse_out <= '1' ;
         else
            pulse_out <= '0' ;
         end if ;
      end if ;
   end process ;

end architecture ;
