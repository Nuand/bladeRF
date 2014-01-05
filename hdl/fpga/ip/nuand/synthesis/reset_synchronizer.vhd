library ieee ;
    use ieee.std_logic_1164.all ;

entity reset_synchronizer is
  generic (
    INPUT_LEVEL     :       std_logic   := '1' ;
    OUTPUT_LEVEL    :       std_logic   := '1'
  ) ;
  port (
    clock           :   in  std_logic ;
    async           :   in  std_logic ;
    sync            :   out std_logic
  ) ;
end entity ;

architecture arch of reset_synchronizer is

begin

    synchronize : process( clock, async )
        variable reg0, reg1 :   std_logic ;
    begin
        if( async = INPUT_LEVEL ) then
            sync <= OUTPUT_LEVEL ;
            reg0 := OUTPUT_LEVEL ;
            reg1 := OUTPUT_LEVEL ;
        elsif( rising_edge( clock ) ) then
            sync <= reg1 ;
            reg1 := reg0 ;
            reg0 := not OUTPUT_LEVEL ;
        end if ;
    end process ;

end architecture ;

