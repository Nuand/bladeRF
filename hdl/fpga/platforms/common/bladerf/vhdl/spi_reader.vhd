-- Copyright (c) 2013 Nuand LLC
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

library ieee;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity spi_reader is
  port (
    clock   :   in  std_logic ;

    sclk    :   out std_logic ;
    miso    :   in  std_logic ;
    mosi    :   out std_logic ;
    enx     :   out std_logic ;
    reset_out : out std_logic
  ) ;
end entity ;

architecture arch of spi_reader is

    constant RESET_CYCLES       : natural := 1000 ;
    constant ENX_WAIT_CYCLES    : natural := 10 ;
    constant WRITE_BIT_COUNT    : natural := 7 ;
    constant READ_BIT_COUNT     : natural := 8 ;
    constant TOTAL_BIT_COUNT    : natural := WRITE_BIT_COUNT + READ_BIT_COUNT ;

    type command_t is (COMMAND_READ, COMMAND_WRITE) ;
    type fsm_t is (RESET, WAITING, FALLING_SCLK, RISING_SCLK) ;
    signal fsm : fsm_t ;
    constant command : command_t := COMMAND_READ ;
    signal spi_address : unsigned(6 downto 0) := (others =>'0') ;

begin

    reader : process(all)
        variable count : natural range 0 to 1000 := 0 ;
        variable address : unsigned(6 downto 0) := (others =>'0') ;
        variable wdata : unsigned(7 downto 0) ;
        variable rdata : unsigned(7 downto 0) ;
    begin
        if( rising_edge(clock) ) then
            enx <= '0' ;
            reset_out <= '1' ;
            case fsm is

                when RESET =>
                    reset_out <= '0' ;
                    spi_address <= (others =>'0') ;
                    if( count = 0 ) then
                        fsm <= WAITING ;
                        count := ENX_WAIT_CYCLES ;
                    else
                        count := count - 1 ;
                    end if ;

                when WAITING =>
                    sclk <= '1' ;
                    enx <= '1' ;
                    if( spi_address = 127 ) then
                        spi_address <= (others =>'0') ;
                        fsm <= RESET ;
                        count := RESET_CYCLES ;
                    end if ;
                    if( count = 0 ) then
                        count := TOTAL_BIT_COUNT ;
                        fsm <= FALLING_SCLK ;
                        address := spi_address ;
                        spi_address <= spi_address + 1;
                    else
                        count := count - 1 ;
                    end if ;

                when FALLING_SCLK =>
                    sclk <= '0' ;
                    fsm <= RISING_SCLK ;
                    if( command = COMMAND_READ ) then
                        if( count = TOTAL_BIT_COUNT ) then
                            mosi <= '0' ;
                        elsif( count < TOTAL_BIT_COUNT - WRITE_BIT_COUNT ) then
                            mosi <= '0' ;
                        else
                            mosi <= address(address'high) ;
                            address := shift_left(address,1) ;
                        end if ;
                    else
                        if( count = TOTAL_BIT_COUNT ) then
                            mosi <= '1' ;
                        elsif( count < TOTAL_BIT_COUNT - WRITE_BIT_COUNT ) then
                            mosi <= wdata(wdata'high) ;
                            wdata := shift_left(wdata,1) ;
                        else
                            mosi <= address(address'high) ;
                            address := shift_left(address,1) ;
                        end if ;

                    end if ;

                when RISING_SCLK =>
                    sclk <= '1' ;
                    if( command = COMMAND_READ ) then
                        if( count < TOTAL_BIT_COUNT - WRITE_BIT_COUNT ) then
                            rdata := rdata(rdata'high-1 downto 0) & miso ;
                        end if ;
                    end if ;
                    if( count = 0 ) then
                        fsm <= WAITING ;
                        count := ENX_WAIT_CYCLES ;
                    else
                        count := count - 1 ;
                        fsm <= FALLING_SCLK ;
                    end if ;

            end case ;
        end if ;
    end process ;

end architecture ;
