library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity async_fifo is
  port (
    -- Global clear
    clear       :   in  std_logic ;

    -- Write side
    w_clock     :   in  std_logic ;
    w_enable    :   in  std_logic ;
    w_data      :   in  std_logic_vector(7 downto 0) ;
    w_empty     :   out std_logic ;
    w_full      :   out std_logic ;

    -- Read side
    r_clock     :   in  std_logic ;
    r_enable    :   in  std_logic ;
    r_data      :   out std_logic_vector(7 downto 0) ;
    r_empty     :   out std_logic ;
    r_full      :   out std_logic
  ) ;
end entity ; -- async_fifo

architecture arch of async_fifo is

    -- Clear signals for their respective sides
    signal r_clear : std_logic ;
    signal w_clear : std_logic ;

    type status_t is record
        address : natural range 0 to 1023 ;
        count   : natural range 0 to 1023 ;
    end record ;

begin

end architecture ; -- arch