library ieee ;
    use ieee.std_logic_1164.all ;

-- Utility package
package util is

    procedure nop( signal clock : in std_logic ; count : in natural ) ;

end package ;

package body util is

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge( clock ) ;
        end loop ;
    end procedure ;

end package body ;

