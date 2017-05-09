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

    attribute ALTERA_ATTRIBUTE  : string;
    attribute PRESERVE          : boolean;

    signal reg0, reg1   : std_logic;

    attribute ALTERA_ATTRIBUTE of arch  : architecture is "-name SDC_STATEMENT ""set_false_path -to [get_registers {*reset_synchronizer:*|*}] """;
    attribute ALTERA_ATTRIBUTE of reg0  : signal is "-name SYNCHRONIZER_IDENTIFICATION ""FORCED IF ASYNCHRONOUS""";
    attribute ALTERA_ATTRIBUTE of reg1  : signal is "-name SYNCHRONIZER_IDENTIFICATION ""FORCED IF ASYNCHRONOUS""";
    attribute PRESERVE of reg0          : signal is TRUE;
    attribute PRESERVE of reg1          : signal is TRUE;
    attribute PRESERVE of sync          : signal is TRUE;

begin

    synchronize : process( clock, async )
    begin
        if( async = INPUT_LEVEL ) then
            sync <= OUTPUT_LEVEL ;
            reg0 <= OUTPUT_LEVEL ;
            reg1 <= OUTPUT_LEVEL ;
        elsif( rising_edge( clock ) ) then
            sync <= reg1 ;
            reg1 <= reg0 ;
            reg0 <= not OUTPUT_LEVEL ;
        end if ;
    end process ;

end architecture ;

