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

entity uart_bridge is
  port (
    -- UART Interface
    uart_clock  :   in  std_logic ;
    uart_reset  :   in  std_logic ;
    uart_enable :   in  std_logic ;
    uart_rxd    :   out std_logic ;
    uart_txd    :   out std_logic ;

    -- SPI Interface
    spi_clock   :   in  std_logic ;
    spi_reset   :   in  std_logic ;
    spi_sclk    :   out std_logic ;
    spi_miso    :   in  std_logic ;
    spi_mosi    :   out std_logic ;
    spi_csn     :   out std_logic
  ) ;
end entity ; -- uart_bridge

architecture arch of uart_bridge is

    -- UART to SPI FIFO signals
    signal u2s_write        :   std_logic ;
    signal u2s_full         :   std_logic ;
    signal u2s_data         :   std_logic_vector(7 downto 0) ;

    -- SPI to UART FIFO signals
    signal s2u_read         :   std_logic ;
    signal s2u_empty        :   std_logic ;
    signal s2u_data         :   std_logic_vector(7 downto 0) ;

begin

    -- UART interface
    U_uart : entity work.uart
      port map (
        -- Control signals
        clock       => uart_clock,
        reset       => uart_reset,
        enable      => uart_enable,

        -- External UART signals
        rs232_rxd   => uart_rxd,
        rs232_txd   => uart_txd,

        -- FIFO signals for UART to FIFO from external interface
        txd_we      => u2s_write,
        txd_full    => u2s_full,
        txd_wdata   => u2s_data,

        -- FIFO signals for FIFO to UART to be transmitted back
        rxd_re      => s2u_read,
        rxd_empty   => s2u_empty,
        rxd_rdata   => s2u_data
      ) ;

    -- Dual clock FIFO for UART -> SPI comms
    U_u2s_fifo : entity work.async_fifo
      port map (
        -- Write side: UART
        w_clock     => uart_clock,
        w_enable    => u2s_write,
        w_data      => u2s_data,
        w_empty     => open,
        w_full      => u2s_full,

        -- Read side: SPI
        r_clock     => spi_clock,
        r_enable    => spi_read,
        r_data      => spi_data,
        r_empty     => spi_empty,
        r_full      => open
      ) ;

    -- Dual clock FIFO for SPI -> UART comms
    U_s2u_fifo : entity work.async_fifo
      port map (
        -- Write side: SPI
        w_clock     => spi_clock,
        w_enable    => spi_readback_wen,
        w_data      => spi_readback_data,
        w_empty     => open,
        w_full      => s2u_full,

        -- Read side: UART
        r_clock     => uart_clock,
        r_enable    => s2u_read,
        r_data      => s2u_data,
        r_empty     => s2u_empty,
        r_full      => open
      ) ;

    -- LMS SPI interface
    U_lms_spi : entity lms_spi
      port map (
        -- Control signals
        clock           => spi_clock,
        reset           => spi_reset,

        -- SPI data stream
        spi_data        => spi_data,
        spi_read        => spi_read,
        spi_empty       => spi_empty,

        -- SPI readback data stream
        readback_data   => spi_readback_data,
        readback_enable => spi_readback_wen
      ) ;

end architecture ; -- arch
