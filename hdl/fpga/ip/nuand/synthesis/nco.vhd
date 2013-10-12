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

package nco_p is

    type nco_input_t is record
        dphase  :   signed(15 downto 0) ;
        valid   :   std_logic ;
    end record ;

    type nco_output_t is record
        re      :   signed(15 downto 0) ;
        im      :   signed(15 downto 0) ;
        valid   :   std_logic ;
    end record ;

end package ; -- nco_p

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

library work ;
    use work.cordic_p.all ;
    use work.nco_p.all ;

entity nco is
  port (
    clock           :   in  std_logic ;
    reset           :   in  std_logic ;
    inputs          :   in  nco_input_t ;
    outputs         :   out nco_output_t
  ) ;
end entity ; -- nco

architecture arch of nco is

    signal phase : signed(15 downto 0) ;

    signal cordic_inputs : cordic_xyz_t ;
    signal cordic_outputs : cordic_xyz_t ;

begin

    accumulate_phase : process(clock, reset)
        variable temp : signed(15 downto 0) ;
    begin
        if( reset = '1' ) then
            phase <= (others =>'0') ;
        elsif( rising_edge( clock ) ) then
            if( inputs.valid = '1' ) then
                temp := phase + inputs.dphase ;
                if( temp > 4096 ) then
                    temp := temp - 8192 ;
                elsif( temp < -4096 ) then
                    temp := temp + 8192 ;
                end if ;
                phase <= temp ;
            end if ;
        end if ;
    end process ;

    cordic_inputs <= ( x => to_signed(1234,16), y => (others =>'0'),  z => phase, valid => inputs.valid ) ;

    U_cordic : entity work.cordic
      port map (
        clock   =>  clock,
        reset   =>  reset,
        mode    => CORDIC_ROTATION,
        inputs  => cordic_inputs,
        outputs => cordic_outputs
      ) ;

    outputs.re <= cordic_outputs.x ;
    outputs.im <= cordic_outputs.y ;
    outputs.valid <= cordic_outputs.valid ;

end architecture ; -- arch
