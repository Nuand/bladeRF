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

entity uart is
  port (
    -- FPGA internal interface to the TX and RX FIFOs
    clock           :   in  std_logic ;
    reset           :   in  std_logic ;
    enable          :   in  std_logic ;

    -- RS232 interface
    rs232_rxd       :   out std_logic ;
    rs232_txd       :   in  std_logic ;

    -- UART -> FIFO interface
    txd_we          :   out std_logic ;
    txd_full        :   in  std_logic ;
    txd_data        :   out std_logic_vector(7 downto 0) ;

    -- FIFO -> UART interface
    rxd_re          :   out std_logic ;
    rxd_empty       :   in  std_logic ;
    rxd_data        :   in  std_logic_vector(7 downto 0)
  ) ;
end entity ; -- uart

architecture arch of uart is

    -- TODO: Make this a generic instead of a constant
    constant CLOCKS_PER_BIT :   natural := 10 ;

    -- Send and receive state machine definitions
    type send_fsm_t is (IDLE, SEND_START, SEND_DATA, SEND_STOP ) ;
    type receive_fsm_t is (WAIT_FOR_START, CENTER_START, SAMPLE_DATA, WAIT_FOR_STOP) ;

    -- FSM signals
    signal send_fsm     : send_fsm_t ;
    signal receive_fsm  : receive_fsm_t ;

    -- Synchronization signals for asynchronous input
    signal reg_txd      : std_logic ;
    signal sync_txd     : std_logic ;

begin

    -- Synchronize asynchronous inputs
    reg_txd <= rs232_txd when rising_edge( clock ) ;
    sync_txd <= reg_txd when rising_edge( clock ) ;

    -- Receive data from the UART and put it in the FIFO
    receive_from_uart : process(all)
        variable bits           :   std_logic_vector(7 downto 0) ;
        variable bit_count      :   natural range 0 to bits'length ;
        variable clock_count    :   natural range 0 to CLOCKS_PER_BIT ;
        variable txd_delay      :   std_logic ;
    begin
        if( reset = '1' ) then
            receive_fsm <= WAIT_FOR_START ;
            txd_we <= '0' ;
            txd_data <= (others =>'0') ;
            bits := (others =>'0') ;
            bit_count := 0 ;
            clock_count := 0 ;
            txd_delay := '0' ;
        elsif( rising_edge(clock) ) then
            if( enable = '0' ) then
                txd_we <= '0' ;
                txd_data <= (others =>'0') ;
                bits := (others =>'0') ;
                bit_count := 0 ;
                clock_count := 0 ;
                txd_delay := '0' ;
            else
                txd_we <= '0' ;
                case receive_fsm is
                    -- Wait for the start signal
                    when WAIT_FOR_START =>
                        if( sync_txd = '0' and txd_delay = '1' ) then
                            receive_fsm <= CENTER_START ;
                            clock_count := CLOCKS_PER_BIT/2-1 ;
                        end if ;
                        txd_delay := sync_txd ;

                    -- Center the start bit
                    when CENTER_START =>
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            receive_fsm <= SAMPLE_DATA ;
                            clock_count := CLOCKS_PER_BIT ;
                            bit_count := bits'length ;
                        end if ;

                    -- Sample the bits
                    when SAMPLE_DATA =>
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            bits := sync_txd & bits(bits'high downto 1) ;
                            bit_count := bit_count - 1 ;
                            clock_count := CLOCKS_PER_BIT ;
                            if( bit_count = 0 ) then
                                receive_fsm <= WAIT_FOR_STOP ;
                                txd_data <= bits ;
                                txd_we <= '1' ;
                            end if ;
                        end if ;

                    -- Wait for the stop bit
                    when WAIT_FOR_STOP =>
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            receive_fsm <= WAIT_FOR_START ;
                            assert sync_txd = '1' report "STOP bit not found!" severity error ;
                        end if ;

                    when others =>
                end case ;
            end if ;
        end  if ;
    end process ;

    -- Send data to the UART from the FIFO
    send_to_uart : process(all)
        variable bits           : std_logic_vector(7 downto 0) ;
        variable bit_count      : natural range 0 to bits'length ;
        variable clock_count    : natural range 0 to CLOCKS_PER_BIT ;
    begin
        if( reset = '1' ) then
            send_fsm    <= IDLE ;
            rxd_re      <= '0' ;
            rs232_rxd   <= '1' ;
            bit_count   := 0 ;
            clock_count := 0 ;
            bits        := (others =>'0') ;
        elsif( rising_edge(clock) ) then
            if( enable = '0' ) then
                send_fsm    <= IDLE ;
                rxd_re      <= '0' ;
                rs232_rxd   <= '1' ;
                bit_count   := 0 ;
                clock_count := 0 ;
                bits        := (others =>'0') ;
            else
                rxd_re <= '0' ;
                case send_fsm is
                    -- Wait until we have something to send
                    when IDLE =>
                        rs232_rxd <= '1' ;
                        if( rxd_empty = '0' ) then
                            send_fsm    <= SEND_START ;
                            rxd_re      <= '1' ;
                            bit_count   := bits'length ;
                            clock_count := CLOCKS_PER_BIT ;
                        end if ;

                    -- Send the start bit
                    when SEND_START =>
                        rs232_rxd <= '0' ;
                        -- bits        := rxd_data ;
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            send_fsm    <= SEND_DATA ;
                            clock_count := CLOCKS_PER_BIT ;
                        elsif( clock_count = 9 ) then
                            bits := rxd_data ;
                        end if ;
                    -- Send the data
                    when SEND_DATA =>
                        rs232_rxd   <= bits(0) ;
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            bits        := '0' & bits(bits'high downto 1) ;
                            bit_count   := bit_count - 1 ;
                            clock_count := CLOCKS_PER_BIT ;
                            if( bit_count = 0 ) then
                                send_fsm <= SEND_STOP ;
                            end if ;
                        end if ;

                    -- Send the stop bit
                    when SEND_STOP =>
                        rs232_rxd   <= '1' ;
                        clock_count := clock_count - 1 ;
                        if( clock_count = 0 ) then
                            send_fsm <= IDLE ;
                        end if ;

                    -- Weird
                    when others =>
                        send_fsm    <= IDLE ;
                        rs232_rxd   <= '1' ;
                        bit_count   := 0 ;
                        clock_count := 0 ;
                        bits        := (others =>'0') ;
                end case ;
            end if ;
        end if ;
    end process ;

end architecture ; -- arch
