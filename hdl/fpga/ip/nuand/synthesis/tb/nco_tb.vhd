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

library work ;
    use work.cordic_p.all ;
    use work.nco_p.all ;

entity nco_tb is
end entity ; -- nco_tb

architecture arch of nco_tb is

    signal clock    :   std_logic := '1' ;
    signal reset    :   std_logic := '1' ;

    signal inputs   :   nco_input_t := ( dphase => (others =>'0'), valid => '0' );
    signal outputs  :   nco_output_t ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge( clock ) ;
        end loop ;
    end procedure ;

begin

    clock <= not clock after 1 ns ;

    U_nco : entity work.nco
      port map (
        clock   => clock,
        reset   => reset,
        inputs  => inputs,
        outputs => outputs
      ) ;

    tb : process
        variable up_downx : boolean := true ;
    begin
        reset <= '1' ;
        nop( clock, 10 ) ;

        reset <= '0' ;
        nop( clock, 10 ) ;

        for i in 0 to 100000 loop
            if( inputs.dphase = 4096 ) then
                up_downx := false ;
            elsif( inputs.dphase = -4096 ) then
                up_downx := true ;
            end if ;

            if( up_downx ) then
                inputs.dphase <= inputs.dphase + 1 ;
            else
                inputs.dphase <= inputs.dphase - 1 ;
            end if ;

            inputs.valid <= '1' ;
            wait until rising_edge( clock ) ;
            inputs.valid <= '0' ;

            nop( clock, 10 ) ;

        end loop ;

        report "-- End of Simulation --" severity failure ;
    end process ;

end architecture ; -- arch
