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

entity fsk_tb is
end entity ; -- fsk_tb

architecture arch of fsk_tb is

    signal clock : std_logic := '1' ;
    signal reset : std_logic := '1' ;

    signal tx_symbol : std_logic := '0' ;
    signal tx_valid  : std_logic := '0' ;

    signal tx_sig_real : signed(15 downto 0) ;
    signal tx_sig_imag : signed(15 downto 0) ;
    signal tx_sig_valid : std_logic ;

    signal rx_symbol : signed(15 downto 0) ;
    signal rx_valid : std_logic ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge( clock ) ;
        end loop ;
    end procedure ;

begin

    clock <= not clock after 1 ns ;

    U_fsk_modulator : entity work.fsk_modulator
      port map (
        clock           => clock,
        reset           => reset,
        symbol          => tx_symbol,
        symbol_valid    => tx_valid,
        out_real        => tx_sig_real,
        out_imag        => tx_sig_imag,
        out_valid       => tx_sig_valid
      ) ;

    U_fsk_demodulator : entity work.fsk_demodulator
      port map (
        clock       => clock,
        reset       => reset,
        in_real     => tx_sig_real,
        in_imag     => tx_sig_imag,
        in_valid    => tx_sig_valid,
        out_ssd     => rx_symbol,
        out_valid   => rx_valid
      ) ;

    tb : process
        variable s1, s2 : natural ;
        variable random : real ;
    begin
        reset <= '1' ;
        nop( clock, 10 ) ;

        reset <= '0' ;
        nop( clock, 10 ) ;

        s1 := 1234 ;
        s2 := 2345 ;
        for i in 0 to 10000 loop
            uniform( s1, s2, random ) ;
            if( random >= 0.5 ) then
                tx_symbol <= '1' ;
            else
                tx_symbol <= '0' ;
            end if ;
            tx_valid <= '1' ;
            nop( clock, 50 ) ;
        end loop ;

        report "-- End of Simulation --" severity failure ;
    end process ;

end architecture ; -- arch
