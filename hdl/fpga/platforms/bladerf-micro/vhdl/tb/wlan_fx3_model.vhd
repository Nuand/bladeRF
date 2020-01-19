-- Copyright (c) 2019 Nuand LLC
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
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;
    use ieee.math_complex.all;

library work;
    use work.util.all;

architecture wlan_micro_packet_dma of fx3_model is

    constant PCLK_HALF_PERIOD       : time      := 1 sec * (1.0/100.0e6/2.0);
    constant START_COUNT            : natural   := 32;
    constant BLOCKS_PER_ITERATION   : natural   := 4;
    constant ITERATIONS             : natural   := 4;

    -- Control mapping
    alias dma0_rx_ack   is fx3_ctl( 0);
    alias dma1_rx_ack   is fx3_ctl( 1);
    alias dma2_tx_ack   is fx3_ctl( 2);
    alias dma3_tx_ack   is fx3_ctl( 3);
    alias dma_rx_enable is fx3_ctl( 4);
    alias dma_tx_enable is fx3_ctl( 5);
    alias dma_idle      is fx3_ctl( 6);
    alias system_reset  is fx3_ctl( 7);
    alias dma0_rx_reqx  is fx3_ctl( 8);
    alias dma1_rx_reqx  is fx3_ctl(12); -- due to 9 being connected to dclk
    alias dma2_tx_reqx  is fx3_ctl(10);
    alias dma3_tx_reqx  is fx3_ctl(11);

    type gpif_state_t is (IDLE, TX_META, TX_SAMPLES, RX_SAMPLES);
    signal gpif_state_rx, gpif_state_tx : gpif_state_t;

    signal rx_done  : boolean := false;
    signal tx_done  : boolean := false;

    signal rx_data  : std_logic_vector(31 downto 0);
    signal tx_data  : std_logic_vector(31 downto 0);

    function data_gen (count : natural) return std_logic_vector is
        variable msw, lsw : std_logic_vector(15 downto 0);
    begin
        msw := (others =>'0');--std_logic_vector(to_signed(count+1, 16));
        lsw := std_logic_vector(to_signed(count, 16));

        return (msw & lsw);
    end function data_gen;

    function data_check (count : natural ; rxdata : std_logic_vector(31 downto 0)) return boolean is
    begin
        return (rxdata = data_gen(count));
    end function data_check;

    type integer_array_t is array(natural range <>) of std_logic_vector(31 downto 0);
    constant TX_PACKET_54_ONE : integer_array_t := (
       x"00000000", x"00020000", x"009a000a", x"00000000", x"000000D4",
       x"e110b470", x"000012e5");

    constant TX_PACKET_54_TWO : integer_array_t := (
       x"00000240", x"00020001", x"009a0036", x"00000000", x"02000207",
       x"04000300", x"06000500", x"08000700", x"0a000900", x"0c000b00",
       x"0e000d00", x"0f010f00", x"02010101", x"04010301", x"06010501",
       x"08010701", x"0a010901", x"0c010b01");

    constant TX_PACKET_54_THREE : integer_array_t := (
       x"00000340", x"00020002", x"009a0036", x"00000000", x"02000207",
       x"04000300", x"06000500", x"08000700", x"0a000900", x"0c000b00",
       x"0e000d00", x"0f010f00", x"02010101", x"04010301", x"06010501",
       x"08010701" , x"0a010901" , x"0c010b01");

    constant TX_PACKET_54_FOUR : integer_array_t := (
       x"00000340", x"00020001", x"009a0036", x"00000000", x"02000207",
       x"04000300", x"06000500", x"08000700", x"0a000900", x"0c000b00",
       x"0e000d00", x"0f010f00", x"02010101", x"04010301", x"06010501",
       x"08010701", x"0a010901", x"0c010b01");

begin

    -- DCLK which isn't used
    fx3_ctl(9) <= '0';

    fx3_ctl(3 downto 0) <= (others => 'Z');

    -- Create a 100MHz clock output
    fx3_pclk <= not fx3_pclk after PCLK_HALF_PERIOD when (not rx_done or not tx_done) else '0';

    -- Doneness
    done <= rx_done and tx_done;

    rx_sample_stream : process
        constant BLOCK_SIZE     : natural := 512;
        variable count          : natural := START_COUNT;
        variable req_time       : time;
    begin
        gpif_state_rx   <= IDLE;
        dma0_rx_reqx    <= '1';
        dma1_rx_reqx    <= '1';
        dma_rx_enable   <= '0';
        rx_data         <= (others => 'U');

        wait until rising_edge(fx3_pclk) and system_reset = '0';

        nop(fx3_pclk, 1000);

        wait until rising_edge(fx3_pclk) and fx3_rx_en = '1';

        wait for 30 us;

        for j in 1 to ITERATIONS loop
            dma_rx_enable <= '1';

            for i in 1 to BLOCKS_PER_ITERATION loop
                dma0_rx_reqx    <= '0';
                req_time        := now;
                wait until rising_edge( fx3_pclk ) and dma0_rx_ack = '1';
                wait until rising_edge( fx3_pclk );
                dma0_rx_reqx    <= '1';

                report "RX iteration " & to_string(j) & " block " & to_string(i) & " delay " & to_string(now - req_time);

                for i in 1 to BLOCK_SIZE loop
                    gpif_state_rx   <= RX_SAMPLES;
                    rx_data         <= fx3_gpif;
                    wait until rising_edge( fx3_pclk );

                    --assert data_check(count, rx_data) severity failure;

                    count := (count + 1) mod 2048;
                    gpif_state_rx   <= IDLE;
                    rx_data         <= (others => 'X');
                end loop;
            end loop;

            dma_rx_enable <= '0';
            wait for 10 us;
        end loop;

        report "Done with RX sample stream";
        rx_done <= true;
        wait;
    end process;

    tx_sample_stream : process
        constant BLOCK_SIZE     : natural := 512;
        variable count          : natural := START_COUNT;
        variable timestamp_cntr : natural := 100;
        variable header_len     : natural := 0;
        variable packet_len     : natural := 0;
        variable data_out       : std_logic_vector(31 downto 0);
        variable req_time       : time;
    begin
        gpif_state_tx   <= IDLE;
        dma2_tx_reqx    <= '1';
        dma3_tx_reqx    <= '1';
        dma_tx_enable   <= '0';
        fx3_gpif        <= (others =>'Z');

        wait until rising_edge(fx3_pclk) and system_reset = '0';

        nop(fx3_pclk, 1000);

        wait until rising_edge(fx3_pclk) and fx3_tx_en = '1';

        wait for 10 us;

        for k in 1 to ITERATIONS loop
            dma_tx_enable <= '1';

            for j in 1 to BLOCKS_PER_ITERATION loop
                header_len      := 0;
                dma3_tx_reqx    <= '0';
                req_time        := now;
                wait until rising_edge( fx3_pclk ) and dma3_tx_ack = '1';
                wait until rising_edge( fx3_pclk );
                wait until rising_edge( fx3_pclk );
                wait until rising_edge( fx3_pclk );
                dma3_tx_reqx    <= '1';

                report "TX iteration " & to_string(k) & " block " & to_string(j) & " delay " & to_string(now - req_time);


                if( fx3_tx_meta_en = '1') then
                    header_len := 4;

                    for i in 1 to 4 loop
                        case (i) is
                            when 1 =>
                                if( j = 1) then
                                   packet_len      := TX_PACKET_54_ONE'length;
                                elsif( j = 2 ) then
                                   packet_len      := TX_PACKET_54_TWO'length;
                                elsif( j = 3 ) then
                                   packet_len      := TX_PACKET_54_THREE'length;
                                else
                                   packet_len      := TX_PACKET_54_FOUR'length;
                                end if;
                                data_out := x"1000" & std_logic_vector(to_unsigned(packet_len, 16));
                            when 2 =>
                                --data_out(31 downto 0) := std_logic_vector(to_signed(timestamp_cntr, 32));
                                --timestamp_cntr := timestamp_cntr + 300;
                                data_out := (others => '0');
                            when 3 =>
                                data_out := (others => '0');
                            when 4 =>
                                data_out := (others => '1');
                        end case;

                        gpif_state_tx   <= TX_META;
                        fx3_gpif        <= data_out;
                        tx_data         <= data_out;
                        wait until rising_edge( fx3_pclk );

                        gpif_state_tx   <= IDLE;
                        tx_data         <= (others => 'X');
                    end loop;
                end if;
                count := 0;

                for i in 0 to packet_len-1 loop
                    gpif_state_tx   <= TX_SAMPLES;
                    if( j = 1 ) then
                       data_out        := std_logic_vector(TX_PACKET_54_ONE(i));
                    elsif( j = 2 ) then
                       data_out        := std_logic_vector(TX_PACKET_54_TWO(i));
                    elsif( j = 3 ) then
                       data_out        := std_logic_vector(TX_PACKET_54_THREE(i));
                    else
                       data_out        := std_logic_vector(TX_PACKET_54_FOUR(i));
                    end if;
                    fx3_gpif        <= data_out(31 downto 0);
                    tx_data         <= data_out(31 downto 0);
                    wait until rising_edge( fx3_pclk );

                    count           := (count + 1) mod 2048;
                    gpif_state_tx   <= IDLE;
                    tx_data         <= (others => 'X');
                end loop;

                fx3_gpif <= (others =>'Z');
                nop(fx3_pclk, 10);
            end loop;

            wait for 8 ms;
            dma_tx_enable <= '0';
            wait for 10 us;
        end loop;

       report "Done with TX sample stream";
       tx_done <= true;
       wait;
    end process;

    reset_system : process
    begin
        system_reset <= '1';
        dma_idle <= '0';
        nop( fx3_pclk, 100 );
        system_reset <= '0';
        nop( fx3_pclk, 10 );
        dma_idle <= '1';
        wait;
    end process;

    -- TODO: UART Interface
    fx3_uart_txd <= '1';
    fx3_uart_cts <= '1';

    assert (gpif_state_tx = IDLE and gpif_state_rx /= IDLE)
        or (gpif_state_rx = IDLE and gpif_state_tx /= IDLE)
        or (gpif_state_rx = IDLE and gpif_state_tx = IDLE)
    report "gpif_state_rx and gpif_state_tx cannot both be non-idle"
    severity failure;

end architecture wlan_micro_packet_dma;
