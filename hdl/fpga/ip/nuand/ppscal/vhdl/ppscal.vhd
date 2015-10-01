-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

-- -----------------------------------------------------------------------------
-- Entity:      ppscal
-- Description:
-- Standard:    VHDL-2008
-- -----------------------------------------------------------------------------
entity ppscal is
--    generic(
--
--    );
    port(
        -- Physical Interface
        ref_1pps        :   in  std_logic;
        tcxo_clock      :   in  std_logic;

        -- Avalon-MM Interface
        mm_clock        :   in  std_logic;
        mm_reset        :   in  std_logic;
        mm_rd_req       :   in  std_logic;
        mm_wr_req       :   in  std_logic;
        mm_addr         :   in  std_logic_vector(7 downto 0);
        mm_wr_data      :   in  std_logic_vector(7 downto 0);
        mm_rd_data      :   out std_logic_vector(7 downto 0);
        mm_rd_datav     :   out std_logic;
        mm_wait_req     :   out std_logic;

        -- Avalon Interrupts
        mm_irq          :   out std_logic := '0'

    );
end entity;

architecture arch of ppscal is

    -- Register Map
    constant PPS_CNT_1S_ADDR   : natural := 0*8;
    constant PPS_CNT_10S_ADDR  : natural := 1*8;
    constant PPS_CNT_100S_ADDR : natural := 2*8;
    constant CONTROL_ADDR      : natural := 4*8;
    constant INTERRUPT_ADDR    : natural := 5*8;

    -- Internal variants of 1PPS input
    signal ref_1pps_sync    : std_logic := '0';
    signal ref_1pps_pulse   : std_logic := '0';

    -- Counter data
    type count_t is record
        target        : signed(63 downto 0);
        count         : signed(63 downto 0);
        count_mm_hold : std_logic_vector(63 downto 0);
        count_v       : std_logic;
        reset         : std_logic;
    end record;

    -- to_signed(7,64)
    -- to_signed(77,64)
    -- to_signed(768,64)

    signal pps_1s   : count_t := ( target        => x"0000_0000_0249_F000", --384e5
                                   count         => (others => '0'),
                                   count_mm_hold => (others => '0'),
                                   count_v       => '0',
                                   reset         => '1' );

    signal pps_10s  : count_t := ( target        => x"0000_0000_16E3_6000", -- 384e6
                                   count         => (others => '0'),
                                   count_mm_hold => (others => '0'),
                                   count_v       => '0',
                                   reset         => '1' );

    signal pps_100s : count_t := ( target        => x"0000_0000_E4E1_C000", -- 384e7
                                   count         => (others => '0'),
                                   count_mm_hold => (others => '0'),
                                   count_v       => '0',
                                   reset         => '1' );

    -- Interrupt signals
    signal pps_irq_enable   : std_logic := '0';
    signal pps_irq_clear    : std_logic := '0';

    -- Unsigned version of Avalon-MM address
    signal mm_uaddr : unsigned(mm_addr'range);

begin

    -- Bring the 1PPS signal into the TCXO clock domain
    U_pps_sync : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        ) port map (
            reset       =>  mm_reset,
            clock       =>  tcxo_clock,
            async       =>  ref_1pps,
            sync        =>  ref_1pps_sync
        );

    -- Generate a single-cycle version of the 1PPS signal
    pps_pulse : process( tcxo_clock, mm_reset )
        variable armed : boolean := true;
    begin
        if( mm_reset = '1' ) then
            ref_1pps_pulse <= '0';
            armed := true;
        elsif( rising_edge(tcxo_clock) ) then
            ref_1pps_pulse <= '0';
            if( armed = true ) then
                if( ref_1pps_sync = '1' ) then
                    ref_1pps_pulse <= '1';
                    armed := false;
                end if;
            else
                if( ref_1pps_sync = '0' ) then
                    armed := true;
                end if;
            end if;
        end if;
    end process;

    -- Keep track of how many TCXO clock pulses have occurred during
    -- the previous 1 second.
    U_1s_counter : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_1s.count'length,
            PPS_PULSES      => 1
        )
        port map (
            clock           => mm_clock,
            reset           => pps_1s.reset,
            count           => pps_1s.count,
            count_strobe    => pps_1s.count_v,
            tcxo_clock      => tcxo_clock,
            pps             => ref_1pps_pulse
        );

    -- Keep track of how many TCXO clock pulses have occurred during
    -- the previous 10 seconds.
    U_10s_counter : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_10s.count'length,
            PPS_PULSES      => 10
            )
        port map (
            clock           => mm_clock,
            reset           => pps_10s.reset,
            count           => pps_10s.count,
            count_strobe    => pps_10s.count_v,
            tcxo_clock      => tcxo_clock,
            pps             => ref_1pps_pulse
        );

    -- Keep track of how many TCXO clock pulses have occurred during
    -- the previous 100 seconds.
    U_100s_counter : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_100s.count'length,
            PPS_PULSES      => 100
            )
        port map (
            clock           => mm_clock,
            reset           => pps_100s.reset,
            count           => pps_100s.count,
            count_strobe    => pps_100s.count_v,
            tcxo_clock      => tcxo_clock,
            pps             => ref_1pps_pulse
        );


    -- Interrupt Request
    int_req_proc : process( mm_clock )
        -- Error tolerance on each clock count
        -- Calculated for a goal of < 10 PPB
        -- err_counts = (seconds * nominal_tcxo_freq) * (10 * 10e-9)
        -- TODO: Make this a function?
        constant PPS_1S_ERROR_TOL   : natural := 1;
        constant PPS_10S_ERROR_TOL  : natural := 3;
        constant PPS_100S_ERROR_TOL : natural := 38;

        variable pps_1s_error   : signed(pps_1s.target'range);
        variable pps_10s_error  : signed(pps_10s.target'range);
        variable pps_100s_error : signed(pps_100s.target'range);

        variable updated  : std_logic := '0';
    begin
        if( rising_edge(mm_clock) ) then

            -- We have a new count to check
            updated := pps_1s.count_v or pps_10s.count_v or pps_100s.count_v;

            -- Update errors only when a new count value is present
            if( pps_1s.count_v = '1' ) then
                pps_1s_error   := abs(pps_1s.target - pps_1s.count);
            end if;

            if( pps_10s.count_v = '1' ) then
                pps_10s_error  := abs(pps_10s.target  - pps_10s.count);
            end if;

            if( pps_100s.count_v = '1' ) then
                pps_100s_error := abs(pps_100s.target - pps_100s.count);
            end if;

            -- Generate an interrupt
            if( (pps_irq_enable = '1') and (updated = '1') ) then
                if( pps_1s_error > PPS_1S_ERROR_TOL ) then
                    mm_irq <= '1';
                elsif( pps_10s_error > PPS_10S_ERROR_TOL ) then
                    mm_irq <= '1';
                elsif( pps_100s_error > PPS_100S_ERROR_TOL ) then
                    mm_irq <= '1';
--                else
--                    mm_irq <= '0';
                end if;
--            else
--                mm_irq <= '0';
            end if;

            -- Force a clear, if needed
            if( pps_irq_clear = '1' ) then
                mm_irq <= '0';
            end if;

        end if;
    end process;

    mm_uaddr    <= unsigned(mm_addr);
    mm_wait_req <= '0';

    -- Avalon-MM Read Process
    mm_read_proc : process( mm_clock )
    begin
        if( rising_edge(mm_clock) ) then

            -- Data valid is just a one-cycle delay of read request
            mm_rd_datav <= mm_rd_req;

            case to_integer(mm_uaddr) is

                -- 1 Second Count
                when PPS_CNT_1S_ADDR+0 =>
                    pps_1s.count_mm_hold <= std_logic_vector(pps_1s.count);
                    mm_rd_data <= std_logic_vector(pps_1s.count(7 downto 0));
                when PPS_CNT_1S_ADDR+1 => mm_rd_data <= pps_1s.count_mm_hold(15 downto 8);
                when PPS_CNT_1S_ADDR+2 => mm_rd_data <= pps_1s.count_mm_hold(23 downto 16);
                when PPS_CNT_1S_ADDR+3 => mm_rd_data <= pps_1s.count_mm_hold(31 downto 24);
                when PPS_CNT_1S_ADDR+4 => mm_rd_data <= pps_1s.count_mm_hold(39 downto 32);
                when PPS_CNT_1S_ADDR+5 => mm_rd_data <= pps_1s.count_mm_hold(47 downto 40);
                when PPS_CNT_1S_ADDR+6 => mm_rd_data <= pps_1s.count_mm_hold(55 downto 48);
                when PPS_CNT_1S_ADDR+7 => mm_rd_data <= pps_1s.count_mm_hold(63 downto 56);

                -- 10 Second Count
                when PPS_CNT_10S_ADDR+0 =>
                    pps_10s.count_mm_hold <= std_logic_vector(pps_10s.count);
                    mm_rd_data <= std_logic_vector(pps_10s.count(7 downto 0));
                when PPS_CNT_10S_ADDR+1 => mm_rd_data <= pps_10s.count_mm_hold(15 downto 8);
                when PPS_CNT_10S_ADDR+2 => mm_rd_data <= pps_10s.count_mm_hold(23 downto 16);
                when PPS_CNT_10S_ADDR+3 => mm_rd_data <= pps_10s.count_mm_hold(31 downto 24);
                when PPS_CNT_10S_ADDR+4 => mm_rd_data <= pps_10s.count_mm_hold(39 downto 32);
                when PPS_CNT_10S_ADDR+5 => mm_rd_data <= pps_10s.count_mm_hold(47 downto 40);
                when PPS_CNT_10S_ADDR+6 => mm_rd_data <= pps_10s.count_mm_hold(55 downto 48);
                when PPS_CNT_10S_ADDR+7 => mm_rd_data <= pps_10s.count_mm_hold(63 downto 56);

                -- 100 Second Count
                when PPS_CNT_100S_ADDR+0 =>
                    pps_100s.count_mm_hold <= std_logic_vector(pps_100s.count);
                    mm_rd_data <= std_logic_vector(pps_100s.count(7 downto 0));
                when PPS_CNT_100S_ADDR+1 => mm_rd_data <= pps_100s.count_mm_hold(15 downto 8);
                when PPS_CNT_100S_ADDR+2 => mm_rd_data <= pps_100s.count_mm_hold(23 downto 16);
                when PPS_CNT_100S_ADDR+3 => mm_rd_data <= pps_100s.count_mm_hold(31 downto 24);
                when PPS_CNT_100S_ADDR+4 => mm_rd_data <= pps_100s.count_mm_hold(39 downto 32);
                when PPS_CNT_100S_ADDR+5 => mm_rd_data <= pps_100s.count_mm_hold(47 downto 40);
                when PPS_CNT_100S_ADDR+6 => mm_rd_data <= pps_100s.count_mm_hold(55 downto 48);
                when PPS_CNT_100S_ADDR+7 => mm_rd_data <= pps_100s.count_mm_hold(63 downto 56);

                when others => null;
            end case;
        end if;
    end process;

    -- Avalon-MM Write Process
    mm_write_proc : process( mm_clock )
    begin
        if( rising_edge(mm_clock) ) then

            pps_irq_clear <= '0';

            if( mm_wr_req = '1' ) then
                case to_integer(mm_uaddr) is
                    when CONTROL_ADDR =>
                        pps_1s.reset   <= mm_wr_data(0);
                        pps_10s.reset  <= mm_wr_data(1);
                        pps_100s.reset <= mm_wr_data(2);

                    when INTERRUPT_ADDR =>
                        pps_irq_enable <= mm_wr_data(0);

                        pps_irq_clear  <= mm_wr_data(4);

                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

end architecture;
