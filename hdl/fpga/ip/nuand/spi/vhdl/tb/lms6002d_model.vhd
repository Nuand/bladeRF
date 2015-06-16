--
--
-- !!! ======================== [ WARNING ] ======================== !!!
--
--          QUICK-AND-DIRTY TESTBENCH ... SCHEDULED FOR CLEAN UP
--
-- !!! ======================== [ WARNING ] ======================== !!!
--
--


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

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

entity lms6002d_model is
  port (
    -- LMS RX Interface
    rx_clock        :   in      std_logic ;
    rx_clock_out    :   buffer  std_logic ;
    rx_data         :   buffer  signed(11 downto 0) ;
    rx_enable       :   in      std_logic ;
    rx_iq_select    :   buffer  std_logic ;

    -- LMS TX Interface
    tx_clock        :   in      std_logic ;
    tx_data         :   in      signed(11 downto 0) ;
    tx_enable       :   in      std_logic ;
    tx_iq_select    :   in      std_logic ;

    -- LMS SPI Interface
    sclk            :   in      std_logic ;
    sen             :   in      std_logic ;
    sdio            :   in      std_logic ;
    sdo             :   buffer  std_logic ;

    -- LMS Control Interface
    pll_out         :   buffer  std_logic ;
    resetx          :   in      std_logic
  ) ;
end entity ; -- lms6002d_model

architecture arch of lms6002d_model is

    constant SPI_CMD_WIDTH  : positive := 1;
    constant SPI_ADDR_WIDTH : positive := 7;
    constant SPI_DATA_WIDTH : positive := 8;
    constant SPI_MSG_WIDTH  : positive := 16;  -- total number of bits in a SPI message
    constant SPI_NUM_REGS   : positive := 128; -- number of SPI registers

    type spi_registers_t is array( 0 to SPI_NUM_REGS-1 ) of std_logic_vector( SPI_DATA_WIDTH-1 downto 0 );

    signal spi_registers : spi_registers_t := (others => (others => '1') );

begin

    pll_out <= '0' ;

    -- Write transmit samples to a file or drop them on the floor

    -- Read receive samples from a file or predetermined tone
    rx_clock_out <= rx_clock ;

    drive_rx_data : process( resetx, rx_clock )
    begin
        if( resetx = '0' ) then
            rx_data <= (others =>'0') ;
            rx_iq_select <= '0' ;
        elsif( rising_edge(rx_clock) ) then
            if( rx_enable = '1' ) then
                rx_data <= rx_data + 1 ;
                rx_iq_select <= not rx_iq_select ;
            else
                rx_data <= (others =>'0') ;
                rx_iq_select <= '0' ;
            end if ;
        end if ;
    end process ;

    -- Keep track of the SPI register set
    spi_reg_proc : process( sclk )
        variable ptr        : natural range 0 to SPI_MSG_WIDTH-1            := 0;
        variable addr       : std_logic_vector( SPI_ADDR_WIDTH-1 downto 0 ) := (others => '0');
        variable data       : std_logic_vector( SPI_DATA_WIDTH-1 downto 0 ) := (others => '0');

        -- Not going to bother making this generic
        variable cmd        : std_logic := '0';
        constant RD_CMD     : std_logic := '0';
        constant WR_CMD     : std_logic := '1';

        -- Trying this method out as a thought experiment since SPI is isolated.
        -- The main register array is a signal because we may want to use it elsewhere.
        type spi_state_t is ( RX_CMD, RX_ADDR, RX_DATA, TX_DATA );
        variable spi_state : spi_state_t := RX_CMD;
    begin
        sdo <= '0'; -- default
        if( rising_edge(sclk) ) then
            if( sen = '0' ) then
                case( spi_state ) is
                    when RX_CMD =>

                        cmd := sdio;
                        ptr := ptr + 1;

                        if( ptr = SPI_CMD_WIDTH ) then
                            ptr       := 0;
                            spi_state := RX_ADDR;
                        end if;

                    when RX_ADDR =>

                        addr := addr(addr'left-1 downto 0) & sdio;
                        ptr := ptr + 1;

                        if( ptr = SPI_ADDR_WIDTH ) then
                            if( cmd = RD_CMD ) then
                                data := spi_registers(to_integer(unsigned(addr)));
                                ptr  := 0;
                                spi_state := TX_DATA;
                            elsif( cmd = WR_CMD ) then
                                ptr := 0;
                                spi_state := RX_DATA;
                            else
                                report "Unknown command value" severity failure;
                            end if;
                        end if;

                    when RX_DATA =>

                        data := data(data'left-1 downto 0) & sdio;
                        ptr  := ptr + 1;

                        if( ptr = SPI_DATA_WIDTH ) then
                            spi_registers(to_integer(unsigned(addr))) <= data;
                            ptr := 0;
                            spi_state := RX_CMD;
                        end if;

                    when TX_DATA =>

                        sdo <= data(data'left);
                        data := data(data'left-1 downto 0) & "0";
                        ptr := ptr + 1;

                        if( ptr = SPI_DATA_WIDTH ) then
                            ptr := 0;
                            spi_state := RX_CMD;
                        end if;

                end case;

            else
                ptr := 0;
                data := (others => '0');
                addr := (others => '0');
                cmd  := '0';
                spi_state := RX_CMD;
            end if;

        end if;

    end process;

end architecture ; -- arch
