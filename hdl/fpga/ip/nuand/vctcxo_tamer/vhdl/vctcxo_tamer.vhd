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
-- Entity:      vctcxo_tamer
-- Description:
-- Standard:    VHDL-2008
-- -----------------------------------------------------------------------------
entity vctcxo_tamer is
    port(
        -- Physical Interface
        tune_ref        :   in  std_logic;
        vctcxo_clock    :   in  std_logic;

        -- Avalon-MM Interface
        mm_clock        :   in  std_logic;
        mm_reset        :   in  std_logic;
        mm_rd_req       :   in  std_logic;
        mm_wr_req       :   in  std_logic;
        mm_addr         :   in  std_logic_vector(7 downto 0);
        mm_wr_data      :   in  std_logic_vector(7 downto 0);
        mm_rd_data      :   out std_logic_vector(7 downto 0);
        mm_rd_datav     :   out std_logic;
        mm_wait_req     :   out std_logic := '0';

        -- Avalon Interrupts
        mm_irq          :   out std_logic := '0'
    );
end entity;

architecture arch of vctcxo_tamer is

    -- Register Addresses
    constant PPS_CNT_1S_ADDR0   : natural := 16#00#;
    constant PPS_CNT_1S_ADDR1   : natural := 16#01#;
    constant PPS_CNT_1S_ADDR2   : natural := 16#02#;
    constant PPS_CNT_1S_ADDR3   : natural := 16#03#;
    constant PPS_CNT_1S_ADDR4   : natural := 16#04#;
    constant PPS_CNT_1S_ADDR5   : natural := 16#05#;
    constant PPS_CNT_1S_ADDR6   : natural := 16#06#;
    constant PPS_CNT_1S_ADDR7   : natural := 16#07#;
    constant PPS_CNT_10S_ADDR0  : natural := 16#08#;
    constant PPS_CNT_10S_ADDR1  : natural := 16#09#;
    constant PPS_CNT_10S_ADDR2  : natural := 16#0A#;
    constant PPS_CNT_10S_ADDR3  : natural := 16#0B#;
    constant PPS_CNT_10S_ADDR4  : natural := 16#0C#;
    constant PPS_CNT_10S_ADDR5  : natural := 16#0D#;
    constant PPS_CNT_10S_ADDR6  : natural := 16#0E#;
    constant PPS_CNT_10S_ADDR7  : natural := 16#0F#;
    constant PPS_CNT_100S_ADDR0 : natural := 16#10#;
    constant PPS_CNT_100S_ADDR1 : natural := 16#11#;
    constant PPS_CNT_100S_ADDR2 : natural := 16#12#;
    constant PPS_CNT_100S_ADDR3 : natural := 16#13#;
    constant PPS_CNT_100S_ADDR4 : natural := 16#14#;
    constant PPS_CNT_100S_ADDR5 : natural := 16#15#;
    constant PPS_CNT_100S_ADDR6 : natural := 16#16#;
    constant PPS_CNT_100S_ADDR7 : natural := 16#17#;
    -- Reserved: 0x18 - 0x1F
    constant CONTROL_ADDR      : natural  := 16#20#;
    -- Reserved: 0x21 - 0x27
    constant INTERRUPT_ADDR    : natural  := 16#28#;
    -- Reserved: 0x29 - 0xFF


    type tune_mode_t is ( DISABLED, PPS, \10MHZ\ );

    function unpack( x : std_logic_vector(1 downto 0) ) return tune_mode_t is
        variable rv : tune_mode_t := DISABLED;
    begin
        case( x ) is
            when "01"   => rv := PPS;
            when "10"   => rv := \10MHZ\;
            when others => rv := DISABLED;
        end case;
        return rv;
    end function;


    -- Counter data
    type count_t is record
        target        : signed(63 downto 0);
        count         : signed(63 downto 0);
        count_mm_hold : std_logic_vector(63 downto 0);
        count_v       : std_logic;
        reset         : std_logic;
    end record;

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

    signal ref_1pps         : std_logic   := '0';
    signal ref_10mhz_pps    : std_logic   := '0';
    signal ref_1pps_sync    : std_logic   := '0';
    signal ref_1pps_pulse   : std_logic   := '0';
    signal pps_irq_enable   : std_logic   := '0';
    signal pps_irq_clear    : std_logic   := '0';
    signal tune_mode        : tune_mode_t := DISABLED;

begin

    -- If the input reference is 10 MHz, use it as a clock to increment a
    -- counter that will generate a single pulse every second.
    ref_10mhz_count_proc : process( tune_ref, mm_reset )
        constant REF_10MHZ_RESET_VAL : natural range 0 to 2**24-1 := 10e6;
        variable ref_10mhz_count     : natural range 0 to 2**24-1 := 10e6;
    begin
        if( mm_reset = '1' ) then
            ref_10mhz_pps <= '0';
            ref_10mhz_count := REF_10MHZ_RESET_VAL;
        elsif( rising_edge(tune_ref) ) then
            ref_10mhz_pps <= '0';
            if( tune_mode = \10MHZ\ ) then
                ref_10mhz_count := ref_10mhz_count - 1;
                if( ref_10mhz_count = 0 ) then
                    ref_10mhz_pps   <= '1';
                    ref_10mhz_count := REF_10MHZ_RESET_VAL;
                end if;
            end if;
        end if;
    end process;

    -- Tuning reference MUX
    ref_1pps <= tune_ref      when ( tune_mode = PPS     ) else
                ref_10mhz_pps when ( tune_mode = \10MHZ\ ) else
                '0';

    -- Bring the 1PPS into the VCTCXO clock domain
    U_pps_sync : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        ) port map (
            reset       =>  mm_reset,
            clock       =>  vctcxo_clock,
            async       =>  ref_1pps,
            sync        =>  ref_1pps_sync
        );

    -- Generate a single-cycle version of the 1PPS signal
    pps_pulse : process( vctcxo_clock, mm_reset )
        variable armed : boolean := true;
    begin
        if( mm_reset = '1' ) then
            ref_1pps_pulse <= '0';
            armed := true;
        elsif( rising_edge(vctcxo_clock) ) then
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

    -- Keep track of how many VCTCXO clock pulses have occurred during
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
            vctcxo_clock    => vctcxo_clock,
            pps             => ref_1pps_pulse
        );

    -- Keep track of how many VCTCXO clock pulses have occurred during
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
            vctcxo_clock    => vctcxo_clock,
            pps             => ref_1pps_pulse
        );

    -- Keep track of how many VCTCXO clock pulses have occurred during
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
            vctcxo_clock    => vctcxo_clock,
            pps             => ref_1pps_pulse
        );


    -- Interrupt Request
    int_req_proc : process( mm_clock )
        -- Error tolerance on each clock count
        -- Calculated for a goal of < 10 PPB
        -- err_counts = (seconds * nominal_vctcxo_freq) * (10 * 10e-9)
        -- TODO: Make this a function?
        constant PPS_1S_ERROR_TOL   : signed(pps_1s.target'range)   := to_signed(1  , pps_1s.target'length);
        constant PPS_10S_ERROR_TOL  : signed(pps_10s.target'range)  := to_signed(4  , pps_10s.target'length);
        constant PPS_100S_ERROR_TOL : signed(pps_100s.target'range) := to_signed(38 , pps_100s.target'length);

    begin
        if( rising_edge(mm_clock) ) then

            if( (pps_1s.count_v = '1') and (pps_irq_enable = '1') ) then
                if( abs(pps_1s.target - pps_1s.count) > PPS_1S_ERROR_TOL ) then
                    mm_irq <= '1';
                end if;
            end if;

            if( (pps_10s.count_v = '1') and (pps_irq_enable = '1') ) then
                if( abs(pps_10s.target - pps_10s.count) > PPS_10S_ERROR_TOL ) then
                    mm_irq <= '1';
                end if;
            end if;

            if( (pps_100s.count_v = '1') and (pps_irq_enable = '1') ) then
                if( abs(pps_100s.target - pps_100s.count) > PPS_100S_ERROR_TOL ) then
                    mm_irq <= '1';
                end if;
            end if;

            -- Clear an IRQ
            if( pps_irq_clear = '1' ) then
                mm_irq <= '0';
            end if;

        end if;
    end process;


    -- Avalon-MM Read Process
    mm_read_proc : process( mm_clock )
    begin
        if( rising_edge(mm_clock) ) then

            -- Data valid is just a one-cycle delay of read request
            mm_rd_datav <= mm_rd_req;
            mm_wait_req <= '0';

            case to_integer(unsigned(mm_addr)) is

                -- 1 Second Count
                when PPS_CNT_1S_ADDR0 =>
                    pps_1s.count_mm_hold <= std_logic_vector(pps_1s.count);
                    mm_rd_data <= std_logic_vector(pps_1s.count(7 downto 0));
                when PPS_CNT_1S_ADDR1 => mm_rd_data <= pps_1s.count_mm_hold(15 downto 8);
                when PPS_CNT_1S_ADDR2 => mm_rd_data <= pps_1s.count_mm_hold(23 downto 16);
                when PPS_CNT_1S_ADDR3 => mm_rd_data <= pps_1s.count_mm_hold(31 downto 24);
                when PPS_CNT_1S_ADDR4 => mm_rd_data <= pps_1s.count_mm_hold(39 downto 32);
                when PPS_CNT_1S_ADDR5 => mm_rd_data <= pps_1s.count_mm_hold(47 downto 40);
                when PPS_CNT_1S_ADDR6 => mm_rd_data <= pps_1s.count_mm_hold(55 downto 48);
                when PPS_CNT_1S_ADDR7 => mm_rd_data <= pps_1s.count_mm_hold(63 downto 56);

                -- 10 Second Count
                when PPS_CNT_10S_ADDR0 =>
                    pps_10s.count_mm_hold <= std_logic_vector(pps_10s.count);
                    mm_rd_data <= std_logic_vector(pps_10s.count(7 downto 0));
                when PPS_CNT_10S_ADDR1 => mm_rd_data <= pps_10s.count_mm_hold(15 downto 8);
                when PPS_CNT_10S_ADDR2 => mm_rd_data <= pps_10s.count_mm_hold(23 downto 16);
                when PPS_CNT_10S_ADDR3 => mm_rd_data <= pps_10s.count_mm_hold(31 downto 24);
                when PPS_CNT_10S_ADDR4 => mm_rd_data <= pps_10s.count_mm_hold(39 downto 32);
                when PPS_CNT_10S_ADDR5 => mm_rd_data <= pps_10s.count_mm_hold(47 downto 40);
                when PPS_CNT_10S_ADDR6 => mm_rd_data <= pps_10s.count_mm_hold(55 downto 48);
                when PPS_CNT_10S_ADDR7 => mm_rd_data <= pps_10s.count_mm_hold(63 downto 56);

                -- 100 Second Count
                when PPS_CNT_100S_ADDR0 =>
                    pps_100s.count_mm_hold <= std_logic_vector(pps_100s.count);
                    mm_rd_data <= std_logic_vector(pps_100s.count(7 downto 0));
                when PPS_CNT_100S_ADDR1 => mm_rd_data <= pps_100s.count_mm_hold(15 downto 8);
                when PPS_CNT_100S_ADDR2 => mm_rd_data <= pps_100s.count_mm_hold(23 downto 16);
                when PPS_CNT_100S_ADDR3 => mm_rd_data <= pps_100s.count_mm_hold(31 downto 24);
                when PPS_CNT_100S_ADDR4 => mm_rd_data <= pps_100s.count_mm_hold(39 downto 32);
                when PPS_CNT_100S_ADDR5 => mm_rd_data <= pps_100s.count_mm_hold(47 downto 40);
                when PPS_CNT_100S_ADDR6 => mm_rd_data <= pps_100s.count_mm_hold(55 downto 48);
                when PPS_CNT_100S_ADDR7 => mm_rd_data <= pps_100s.count_mm_hold(63 downto 56);

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
                case to_integer(unsigned(mm_addr)) is
                    when CONTROL_ADDR =>
                        -- Reserved    <= mm_wr_data(7 downto 6);
                        tune_mode      <= unpack(mm_wr_data(5 downto 4));
                        -- Reserved    <= mm_wr_data(3);
                        pps_100s.reset <= mm_wr_data(2);
                        pps_10s.reset  <= mm_wr_data(1);
                        pps_1s.reset   <= mm_wr_data(0);

                    when INTERRUPT_ADDR =>
                        -- Reserved    <= mm_wr_data(7 downto 5);
                        pps_irq_clear  <= mm_wr_data(4);
                        -- Reserved    <= mm_wr_data(3 downto 1);
                        pps_irq_enable <= mm_wr_data(0);

                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

end architecture;
