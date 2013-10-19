library ieee;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity signal_generator is
  port (
    clock           :   in      std_logic ;
    reset           :   in      std_logic ;
    enable          :   in      std_logic ;

    mode            :   in      std_logic ;

    sample_i        :   out     signed(11 downto 0) ;
    sample_q        :   out     signed(11 downto 0) ;
    sample_valid    :   buffer  std_logic
  ) ;
end entity ;

architecture arch of signal_generator is

begin

    counter : process(clock, reset)
        variable tock   :   std_logic ;
        variable count  : natural range 0 to 2047 ;
    begin
        if( reset = '1' ) then
            count := 0 ;
            tock := '0' ;
            sample_i <= (others =>'0') ;
            sample_q <= (others =>'0') ;
            sample_valid <= '0' ;
        elsif( rising_edge(clock) ) then
            sample_i <= to_signed( count, sample_i'length) ;
            sample_q <= to_signed(-count, sample_q'length) ;
            if( enable = '1' ) then
                sample_valid <= not sample_valid ;
            else
                sample_valid <= '0' ;
            end if ;

            if( sample_valid = '1' ) then
                if( count < 2047 ) then
                    count := count + 1 ;
                else
                    count := 0 ;
                end if ;
            end if ;
        end if ;
    end process ;

end architecture ;
