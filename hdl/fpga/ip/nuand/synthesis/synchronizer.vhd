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
    sync        :   out std_logic := RESET_LEVEL
  ) ;
end entity ;

architecture arch of synchronizer is

    attribute ALTERA_ATTRIBUTE  : string;
    attribute PRESERVE          : boolean;

    signal reg0, reg1   : std_logic;

    attribute ALTERA_ATTRIBUTE of arch  : architecture is "-name SDC_STATEMENT ""set_false_path -to [get_registers {*synchronizer:*|reg0}] """;
    attribute ALTERA_ATTRIBUTE of reg0  : signal is "-name SYNCHRONIZER_IDENTIFICATION ""FORCED IF ASYNCHRONOUS""";
    attribute ALTERA_ATTRIBUTE of reg1  : signal is "-name SYNCHRONIZER_IDENTIFICATION ""FORCED IF ASYNCHRONOUS""";
    attribute PRESERVE of reg0          : signal is TRUE;
    attribute PRESERVE of reg1          : signal is TRUE;
    attribute PRESERVE of sync          : signal is TRUE;

begin

    synchronize : process( clock, reset )
    begin
        if( reset = '1' ) then
            sync <= RESET_LEVEL ;
            reg0 <= RESET_LEVEL ;
            reg1 <= RESET_LEVEL ;
        elsif( rising_edge( clock ) ) then
            sync <= reg1 ;
            reg1 <= reg0 ;
            reg0 <= async ;
        end if ;
    end process ;

end architecture ;

