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

library nuand ;

entity fx3 is
  port (
    -- FX3 GPIF
    pclk                :   in      std_logic ;
    gpif                :   inout   std_logic_vector(31 downto 0) ;
    ctl                 :   inout   std_logic_vector(12 downto 0) ;

    -- FX3 UART
    rxd                 :   out     std_logic ;
    txd                 :   in      std_logic ;
    cts                 :   in      std_logic ;

    -- USB RX'd data output side
    usb_data_rx         :   out     std_logic_vector(31 downto 0) ;
    usb_data_rx_we      :   out     std_logic ;
    usb_data_rx_clear   :   out     std_logic ;

    -- USB TX data input
    usb_data_tx         :   in      std_logic_vector(31 downto 0)   := (others =>'0') ;
    usb_data_tx_empty   :   in      std_logic                       := '1' ;
    usb_data_tx_re      :   out     std_logic ;
    usb_data_tx_clear   :   out     std_logic ;

    -- UART RX'd data output side
    uart_data_rx        :   out     std_logic_vector(7 downto 0) ;
    uart_data_rx_we     :   out     std_logic ;

    -- UART TX data input
    uart_data_tx        :   in      std_logic_vector(7 downto 0)    := (others =>'0') ;
    uart_data_tx_re     :   out     std_logic ;
    uart_data_tx_empty  :   in      std_logic                       := '1'
  ) ;
end entity ; -- fx3

architecture digital_loopback of fx3 is

    alias rxd_samples_request is ctl(0) ;
    alias rxd_samples_acknowledge is ctl(1) ;
    alias rxd_samples_we is ctl(2) ;
    alias txd_samples_we is ctl(3) ;
    alias gpif_reset is ctl(4) ;

    alias unused_ctl is ctl(12 downto 5) ;

    constant FIFO_DEPTH : positive := 1024 ;
    signal rxd_samples : std_logic_vector(gpif'range) ;
    signal fifo_used : natural range 0 to FIFO_DEPTH ;
    signal fifo_full : std_logic ;
    signal fifo_empty : std_logic ;
    signal fifo_read_en : std_logic ;

begin

    -- USB data goes nowhere
    usb_data_rx         <= (others =>'0') ;
    usb_data_rx_we      <= '0' ;
    usb_data_rx_clear   <= '1' ;
    usb_data_tx_re      <= '0' ;
    usb_data_tx_clear   <= '1' ;

    -- UART loopback
    rxd <= txd when cts = '1' else '0' ;
    uart_data_rx <= (others =>'0') ;
    uart_data_rx_we <= '0' ;
    uart_data_tx_re <= '0' ;

    -- Set the unused control lines to something
    unused_ctl <= (others =>'0') ;

    -- Only drive the GPIF when we've gotten an acknowledgement from the FX3 saying we have the bus
    gpif <= rxd_samples when rxd_samples_acknowledge = '1'
            else (others =>'Z') ;

    -- Control signals that the FX3 drives
    rxd_samples_acknowledge <= 'Z' ;
    txd_samples_we <= 'Z' ;
    rxd_samples_we <= '0' when gpif_reset = '1' else 
                      rxd_samples_acknowledge when rising_edge(pclk) ;

    fifo_read_en <= '0' ;

    request_generator : process(all)
    begin
        if( fifo_used = 0 ) then
            rxd_samples_request <= '0' ;
        elsif( fifo_used > 128 ) then
            if( rxd_samples_request = '0' ) then
                rxd_samples_request <= '1' ;
            else
                if( fifo_used = 1 ) then
                    rxd_samples_request <= '0' ;
                else
                    rxd_samples_request <= '1' ;
                end if ;
            end if ;
        else
            if( rxd_samples_request = '1' ) then
                if( fifo_used = 0 ) then
                    rxd_samples_request <= '0' ;
                end if ;
            end if ;
        end if ;
    end process ;

    -- Synchronous FIFO for loopback
    U_fifo : entity nuand.sync_fifo
      generic map (
        DEPTH       =>  FIFO_DEPTH,
        WIDTH       =>  gpif'length,
        READ_AHEAD  =>  true
      ) port map (
        areset      =>  gpif_reset,
        clock       =>  pclk,
        used_words  =>  fifo_used,
        full        =>  fifo_full,
        empty       =>  fifo_empty,
        data_in     =>  gpif,
        write_en    =>  txd_samples_we,
        data_out    =>  rxd_samples,
        read_en     =>  rxd_samples_acknowledge
      ) ;

end architecture ; -- digital_loopback

architecture inband_scheduler of fx3 is

begin

end architecture ; -- inband_scheduler
