library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity spi_reader_tb is
end entity ;

architecture arch of spi_reader_tb is

    signal clock    :   std_logic   := '1' ;
    signal sclk     :   std_logic ;
    signal miso     :   std_logic := '0' ;
    signal mosi     :   std_logic ;
    signal enx      :   std_logic ;
    signal reset_out : std_logic ;

begin

    clock <= not clock after 1 ns ;
    
    U_spi_reader : entity work.spi_reader
      port map (
        clock   =>  clock,

        sclk    =>  sclk,
        miso    =>  miso,
        mosi    =>  mosi,
        enx     =>  enx,
        reset_out => reset_out
      ) ;

end architecture ;
