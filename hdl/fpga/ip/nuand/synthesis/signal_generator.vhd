library ieee;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity signal_generator is
  port (
    clock           :   in      std_logic ;
    reset           :   in      std_logic ;
    enable          :   in      std_logic ;

    mode            :   in      std_logic ;

    sample_i        :   out     signed(15 downto 0) ;
    sample_q        :   out     signed(15 downto 0) ;
    sample_valid    :   buffer  std_logic
  ) ;
end entity ;

architecture arch of signal_generator is

begin

    counter : process(clock, reset)
        constant COUNT_RESET    :   natural := 256 ;
        variable count12        :   integer range -2047 to 2047 := COUNT_RESET ;
        variable count32        :   unsigned(31 downto 0) ;
    begin
        if( reset = '1' ) then
            count12 := COUNT_RESET ;
            count32 := (others =>'0') ;
            sample_i <= (others =>'0') ;
            sample_q <= (others =>'0') ;
            sample_valid <= '0' ;
        elsif( rising_edge(clock) ) then
            if( mode = '0' ) then
                sample_i <= to_signed( count12, sample_i'length) ;
                sample_q <= to_signed(-count12, sample_q'length) ;
            else
                sample_i <= signed(std_logic_vector(count32(15 downto 0))) ;
                sample_q <= signed(std_logic_vector(count32(31 downto 16))) ;
            end if ;

            if( enable = '0' ) then
                count12 := COUNT_RESET ;
                count32 := (others =>'0') ;
                sample_i <= (others =>'0') ;
                sample_q <= (others =>'0') ;
                sample_valid <= '0' ;
            else
                sample_valid <= not sample_valid ;
                if( sample_valid = '1' ) then
                    if( count12 < 2047 ) then
                        count12 := count12 + 1 ;
                    else
                        count12 := -2047 ;
                    end if ;
                    count32 := count32 + 1 ;
                end if ;
            end if ;

        end if ;
    end process ;

end architecture ;
