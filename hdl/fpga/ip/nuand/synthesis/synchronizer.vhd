library ieee ;
    use ieee.std_logic_1164.all ;

entity synchronizer is
  generic (
    RESET_LEVEL :       std_logic   := '1'
  ) ;
  port (
    reset       :   in  std_logic ;
    clock       :   in  std_logic ;
    async       :   in  std_logic ;
    sync        :   out std_logic
  ) ;
end entity ;

architecture arch of synchronizer is

begin

    synchronize : process( clock, reset )
        variable reg0, reg1 :   std_logic ;
    begin
        if( reset = '1' ) then
            sync <= RESET_LEVEL ;
            reg0 := RESET_LEVEL ;
            reg1 := RESET_LEVEL ;
        elsif( rising_edge( clock ) ) then
            sync <= reg1 ;
            reg1 := reg0 ;
            reg0 := async ;
        end if ;
    end process ;

end architecture ;

