-- Copyright (c) 2015 Nuand LLC
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

entity command_uart is
  port (
    -- FPGA internal interface to the TX and RX FIFOs
    clock           :   in  std_logic ;
    reset           :   in  std_logic ;

    -- RS232 interface
    rs232_sout      :   out std_logic ;
    rs232_sin       :   in  std_logic ;

    -- Avalon-MM interface
    addr            :   in  std_logic_vector(4 downto 0) ;
    dout            :   out std_logic_vector(31 downto 0) ;
    din             :   in  std_logic_vector(31 downto 0) ;
    read            :   in  std_logic ;
    write           :   in  std_logic ;
    waitreq         :   out std_logic ;
    readack         :   out std_logic ;
    irq             :   out std_logic
  ) ;
end entity ; -- command_uart

architecture arch of command_uart is

    -- TODO: Make this a generic instead of a constant
    constant CLOCKS_PER_BIT :   natural := 20 ;

    -- Send and receive state machine definitions
    type send_fsm_t is (IDLE, SEND_START, SEND_DATA, SEND_STOP ) ;
    type receive_fsm_t is (WAIT_FOR_START, CENTER_START, SAMPLE_DATA, WAIT_FOR_STOP) ;

    -- FSM signals
    signal send_fsm     : send_fsm_t ;
    signal receive_fsm  : receive_fsm_t ;

    -- Synchronization signals for asynchronous input
    signal reg_sin      : std_logic ;
    signal sync_sin     : std_logic ;

    signal reg_response : std_logic_vector(127 downto 0) ;
    signal reg_request  : std_logic_vector(127 downto 0) ;

    signal sin_data     : std_logic_vector(7 downto 0) ;
    signal sin_we       : std_logic ;

    signal sout_data    : std_logic_vector(7 downto 0) ;
    signal sout_we      : std_logic ;

    signal rs232_sout_reg : std_logic;

    signal ack : std_logic ;

    type magics_t is array(natural range <>) of std_logic_vector(7 downto 0) ;

    -- These are all the magic header characters
    constant magics : magics_t := (
        std_logic_vector(to_unsigned(character'pos('A'),8)),    -- 8x8
        std_logic_vector(to_unsigned(character'pos('B'),8)),    -- 8x16
        std_logic_vector(to_unsigned(character'pos('C'),8)),    -- 8x32
        std_logic_vector(to_unsigned(character'pos('D'),8)),    -- 8x64
        std_logic_vector(to_unsigned(character'pos('E'),8)),    -- 16x64
        std_logic_vector(to_unsigned(character'pos('K'),8)),    -- 32x32
        std_logic_vector(to_unsigned(character'pos('N'),8)),    -- Legacy
        std_logic_vector(to_unsigned(character'pos('T'),8)),    -- Retune
        std_logic_vector(to_unsigned(character'pos('U'),8))     -- Retune2
    ) ;

    signal command_in : std_logic ;

    signal control  :   std_logic_vector(7 downto 0) := (0 => '1', others =>'0') ;

    alias isr_enable is control(0) ;

begin

    readack <= ack ;
    waitreq <= not(ack or write) ;

    -- Avalon-MM interface for responses
    mm_writes : process(clock)
    begin
        if( rising_edge(clock) ) then
            command_in <= '0' ;
            if( write = '1' ) then
                case to_integer(unsigned(addr)) is
                    when  0| 1| 2| 3 => reg_response( 31 downto   0) <= din ; command_in <= '1' ;
                    when  4| 5| 6| 7 => reg_response( 63 downto  32) <= din ;
                    when  8| 9|10|11 => reg_response( 95 downto  64) <= din ;
                    when 12|13|14|15 => reg_response(127 downto  96) <= din ;
                    when others => control(7 downto 0) <= din(7 downto 0) ;
                end case ;
            end if ;
        end if ;
    end process ;

    send_command : process(clock, reset)
        type state_t is (WAITING_FOR_COMMAND, SEND_BYTE, WAITING_FOR_START, WAITING_FOR_DONE) ;
        variable state : state_t ;
        variable count : natural range 0 to 15 := 15 ;
    begin
        if( reset = '1' ) then
            state := WAITING_FOR_COMMAND ;
            sout_we <= '0' ;
        elsif( rising_edge(clock) ) then
            sout_we <= '0' ;
            sout_data <= reg_response(count*8+7 downto count*8) ;
            case state is
                when WAITING_FOR_COMMAND =>
                    if( command_in = '1' ) then
                        state := SEND_BYTE ;
                        count := 0 ;
                    end if ;

                when SEND_BYTE =>
                    sout_we <= '1' ;
                    state := WAITING_FOR_START ;

                when WAITING_FOR_START =>
                    if( send_fsm /= IDLE ) then
                        state := WAITING_FOR_DONE ;
                    end if ;

                when WAITING_FOR_DONE =>
                    if( send_fsm = IDLE ) then
                        if( count < 15 ) then
                            count := count + 1 ;
                            state := SEND_BYTE ;
                        else
                            state := WAITING_FOR_COMMAND ;
                        end if ;
                    end if ;
            end case ;
        end if ;
    end process ;

    -- Avanlon-MM interface for requests
    mm_reads : process(clock)
    begin
        if( rising_edge(clock) ) then
            ack <= read ;
            if( read = '1' ) then
                case to_integer(unsigned(addr)) is
                    when  0| 1| 2| 3 => dout <= reg_request( 31 downto  0) ;
                    when  4| 5| 6| 7 => dout <= reg_request( 63 downto 32) ;
                    when  8| 9|10|11 => dout <= reg_request( 95 downto 64) ;
                    when 12|13|14|15 => dout <= reg_request(127 downto 96) ;
                    when others => dout(7 downto 0) <= control ;
                end case ;
            end if ;
        end if ;
    end process ;

    receive_command : process(clock, reset)
        type state_t is (WAIT_FOR_MAGIC, CHECK_MAGIC, RECEIVE_COMMAND, INTERRUPT, WAIT_FOR_READ) ;
        variable state : state_t ;
        variable count : natural range 0 to 15 ;
    begin
        if( reset = '1' ) then
            state := WAIT_FOR_MAGIC ;
            count := 0 ;
            irq <= '0' ;
        elsif( rising_edge(clock) ) then
            if( sin_we = '1' ) then
                reg_request(count*8+7 downto count*8) <= sin_data ;
            end if ;
            case state is
                when WAIT_FOR_MAGIC =>
                    if( sin_we = '1' ) then
                        state := CHECK_MAGIC ;
                    end if ;

                when CHECK_MAGIC =>
                    for i in magics'range loop
                        if( magics(i) = reg_request(7 downto 0) ) then
                            state := RECEIVE_COMMAND ;
                            count := count + 1 ;
                        end if ;
                    end loop ;
                    if( count = 0 ) then
                        state := WAIT_FOR_MAGIC ;
                    end if ;

                when RECEIVE_COMMAND =>
                    if( sin_we = '1' ) then
                        if( count < 15 ) then
                            count := count + 1 ;
                        else
                            count := 0 ;
                            if( isr_enable = '1' ) then
                                state := INTERRUPT ;
                            else
                                state := WAIT_FOR_MAGIC ;
                            end if ;
                        end if ;
                    end if ;

                when INTERRUPT =>
                    irq <= '1' ;
                    state := WAIT_FOR_READ ;

                when WAIT_FOR_READ =>
                    if( read = '1' ) then
                        irq <= '0' ;
                        state := WAIT_FOR_MAGIC ;
                    end if ;
            end case ;
        end if ;
    end process ;

    -- Synchronize asynchronous inputs
    reg_sin <= rs232_sin when rising_edge( clock ) ;
    sync_sin <= reg_sin when rising_edge( clock ) ;

    -- Receive data from the UART and put it in the FIFO
    receive_from_uart : process(clock, reset)
        variable bits           :   std_logic_vector(7 downto 0) ;
        variable bit_count      :   natural range 0 to bits'length ;
        variable clock_count    :   natural range 0 to CLOCKS_PER_BIT ;
        variable sin_delay      :   std_logic ;
    begin
        if( reset = '1' ) then
            receive_fsm <= WAIT_FOR_START ;
            sin_we <= '0' ;
            sin_data <= (others =>'0') ;
            bits := (others =>'0') ;
            bit_count := 0 ;
            clock_count := 0 ;
            sin_delay := '0' ;
        elsif( rising_edge(clock) ) then
            sin_we <= '0' ;
            case receive_fsm is
                -- Wait for the start signal
                when WAIT_FOR_START =>
                    if( sync_sin = '0' and sin_delay = '1' ) then
                        receive_fsm <= CENTER_START ;
                        clock_count := CLOCKS_PER_BIT/2-1 ;
                    end if ;
                    sin_delay := sync_sin ;

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
                        bits := sync_sin & bits(bits'high downto 1) ;
                        bit_count := bit_count - 1 ;
                        clock_count := CLOCKS_PER_BIT ;
                        if( bit_count = 0 ) then
                            receive_fsm <= WAIT_FOR_STOP ;
                            sin_data <= bits ;
                            sin_we <= '1' ;
                        end if ;
                    end if ;

                -- Wait for the stop bit
                when WAIT_FOR_STOP =>
                    clock_count := clock_count - 1 ;
                    if( clock_count = 0 ) then
                        receive_fsm <= WAIT_FOR_START ;
                        assert sync_sin = '1' report "STOP bit not found!" severity error ;
                    end if ;

                when others =>
            end case ;
        end if ;
    end process ;

    -- Register outputs
    register_sout : process(clock, reset) is
    begin
        if (reset = '1') then
            rs232_sout <= '1';
        elsif (rising_edge(clock)) then
            rs232_sout <= rs232_sout_reg;
        end if;
    end process register_sout;

    -- Send data to the UART from the FIFO
    send_to_uart : process(clock, reset)
        variable bits           : std_logic_vector(7 downto 0) ;
        variable bit_count      : natural range 0 to bits'length ;
        variable clock_count    : natural range 0 to CLOCKS_PER_BIT ;
    begin
        if( reset = '1' ) then
            send_fsm        <= IDLE ;
            rs232_sout_reg  <= '1' ;
            bit_count       := 0 ;
            clock_count     := 0 ;
            bits            := (others =>'0') ;
        elsif( rising_edge(clock) ) then
            case send_fsm is
                -- Wait until we have something to send
                when IDLE =>
                    rs232_sout_reg  <= '1' ;
                    if( sout_we = '1' ) then
                        send_fsm    <= SEND_START ;
                        bit_count   := bits'length ;
                        clock_count := CLOCKS_PER_BIT ;
                    end if ;

                -- Send the start bit
                when SEND_START =>
                    rs232_sout_reg  <= '0' ;
                    clock_count     := clock_count - 1 ;
                    if( clock_count = 0 ) then
                        send_fsm    <= SEND_DATA ;
                        clock_count := CLOCKS_PER_BIT ;
                    elsif( clock_count = 9 ) then
                        bits        := sout_data ;
                    end if ;
                -- Send the data
                when SEND_DATA =>
                    rs232_sout_reg  <= bits(0) ;
                    clock_count     := clock_count - 1 ;
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
                    rs232_sout_reg  <= '1' ;
                    clock_count     := clock_count - 1 ;
                    if( clock_count = 0 ) then
                        send_fsm    <= IDLE ;
                    end if ;

                -- Weird
                when others =>
                    send_fsm        <= IDLE ;
                    rs232_sout_reg  <= '1' ;
                    bit_count       := 0 ;
                    clock_count     := 0 ;
                    bits            := (others =>'0') ;
            end case ;
        end if ;
    end process ;

end architecture ; -- arch
