library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity ramp is
  port (
    clock   :   in  std_logic ;
    reset   :   in  std_logic ;
    
    ramp_out  :   buffer signed(11 downto 0)
  ) ;
end entity ;

architecture arch of ramp is

begin

    count : process(clock, reset)
    begin
        if( reset = '1' ) then
            ramp_out <= (others =>'0') ;
        elsif( rising_edge(clock) ) then
            ramp_out <= ramp_out + 1 ;
        end if ;
    end process ;

end architecture ;

