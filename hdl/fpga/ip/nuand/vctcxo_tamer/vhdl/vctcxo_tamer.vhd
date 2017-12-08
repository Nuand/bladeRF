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
-- Standard:    VHDL-2008
-- Description:
--   Using a known reference (1PPS or 10 MHz), this module logs the number
--   of VCTCXO clock cycles that have occurred over the previous 1-second,
--   10-second, and 100-second intervals. The actual counts for each interval
--   are compared against the ideal to determine the error. If this error
--   exceeds the given tolerance, an interrupt is sent to the NIOS processor,
--   which reads the error and makes adjustments to the VCTCXO trim DAC as
--   necessary. For best results, all counters should be reset after changing
--   the trim DAC.
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
    constant CONTROL_ADDR       : natural := 16#00#;
    constant PPS_ERR_STATUS     : natural := 16#01#;
    -- Reserved: 0x02 - 0x03
    constant PPS_ERR_1S_ADDR0   : natural := 16#04#;
    constant PPS_ERR_1S_ADDR1   : natural := 16#05#;
    constant PPS_ERR_1S_ADDR2   : natural := 16#06#;
    constant PPS_ERR_1S_ADDR3   : natural := 16#07#;
    -- Reserved: 0x08 - 0x0B
    constant PPS_ERR_10S_ADDR0  : natural := 16#0C#;
    constant PPS_ERR_10S_ADDR1  : natural := 16#0D#;
    constant PPS_ERR_10S_ADDR2  : natural := 16#0E#;
    constant PPS_ERR_10S_ADDR3  : natural := 16#0F#;
    -- Reserved: 0x10 - 0x13
    constant PPS_ERR_100S_ADDR0 : natural := 16#14#;
    constant PPS_ERR_100S_ADDR1 : natural := 16#15#;
    constant PPS_ERR_100S_ADDR2 : natural := 16#16#;
    constant PPS_ERR_100S_ADDR3 : natural := 16#17#;
    -- Reserved: 0x18 - 0xFF

    -- Error tolerance on each clock count, calculated for a goal of < 10 PPB
    --   err_counts = (seconds * nominal_vctcxo_freq) * (10 * 1e-9)
    constant PPS_1S_ERROR_TOL   : signed(31 downto 0) := to_signed(1  , 32);
    constant PPS_10S_ERROR_TOL  : signed(31 downto 0) := to_signed(4  , 32);
    constant PPS_100S_ERROR_TOL : signed(31 downto 0) := to_signed(38 , 32);

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
        target  : signed(63 downto 0);
        count   : signed(63 downto 0);
        error   : signed(31 downto 0);
        error_v : std_logic;
        count_v : std_logic;
    end record;

    signal pps_1s   : count_t := ( target  => x"0000_0000_0249_F000", --384e5
                                   count   => (others => '0'),
                                   error   => (others => '0'),
                                   error_v => '0',
                                   count_v => '0' );

    signal pps_10s  : count_t := ( target  => x"0000_0000_16E3_6000", -- 384e6
                                   count   => (others => '0'),
                                   error   => (others => '0'),
                                   error_v => '0',
                                   count_v => '0' );

    signal pps_100s : count_t := ( target  => x"0000_0000_E4E1_C000", -- 384e7
                                   count   => (others => '0'),
                                   error   => (others => '0'),
                                   error_v => '0',
                                   count_v => '0' );

    -- Asynchronous
    signal ref_1pps                 : std_logic   := '0';

    -- Tune_ref-synchronous signals
    signal tune_ref_reset           : std_logic   := '1';
    signal ref_10mhz_pps            : std_logic   := '0';
    signal tune_ref_mode_update_req : std_logic   := '0';
    signal tune_ref_mode_update_ack : std_logic   := '0';
    signal tune_ref_mode            : tune_mode_t := DISABLED;
    signal tune_ref_mode_hs         : tune_mode_t := DISABLED;

    -- VCTCXO-synchronous signals
    signal ref_1pps_sync            : std_logic   := '0';
    signal ref_1pps_pulse           : std_logic   := '0';
    signal vctcxo_reset             : std_logic   := '1';

    -- System-synchronous signals
    signal mm_control_reg       : std_logic_vector(7 downto 0) := x"21";
    alias  mm_tune_mode         : std_logic_vector(1 downto 0) is mm_control_reg(7 downto 6);
    alias  mm_pps_irq_clear     : std_logic                    is mm_control_reg(5);
    alias  mm_pps_irq_enable    : std_logic                    is mm_control_reg(4);
    alias  mm_vctcxo_reset      : std_logic                    is mm_control_reg(0);

begin

    -- If the input reference is 10 MHz, use it as a clock to increment a
    -- counter that will generate a single pulse every second.
    ref_10mhz_count_proc : process( tune_ref, tune_ref_reset )
        constant REF_10MHZ_RESET_VAL      : natural range 0 to 2**24-1 := 10e6;
        constant REF_10MHZ_PULSE_DURATION : natural range 0 to 2**24-1 := 1e3;
        variable v_10mhz_count : natural range 0 to 2**24-1 := 10e6;
        variable v_tune_mode   : tune_mode_t                := DISABLED;
    begin
        if( tune_ref_reset = '1' ) then
            ref_10mhz_pps <= '0';
            v_10mhz_count := REF_10MHZ_RESET_VAL;
            v_tune_mode   := DISABLED;
        elsif( rising_edge(tune_ref) ) then
            ref_10mhz_pps <= '0';
            if( v_tune_mode /= tune_ref_mode ) then
                v_10mhz_count := REF_10MHZ_RESET_VAL;
            elsif( tune_ref_mode = \10MHZ\ ) then
                v_10mhz_count := v_10mhz_count - 1;
                if( v_10mhz_count = 0 ) then
                    v_10mhz_count := REF_10MHZ_RESET_VAL;
                end if;
                if( v_10mhz_count <= REF_10MHZ_PULSE_DURATION ) then
                    ref_10mhz_pps   <= '1';
                end if;
            end if;
            v_tune_mode := tune_ref_mode;
        end if;
    end process;

    -- Tuning reference MUX
    ref_1pps <= tune_ref      when ( unpack(mm_tune_mode) = PPS     ) else
                ref_10mhz_pps when ( unpack(mm_tune_mode) = \10MHZ\ ) else
                '0';

    -- Bring the 1PPS into the VCTCXO clock domain
    U_pps_sync : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        ) port map (
            reset       =>  vctcxo_reset,
            clock       =>  vctcxo_clock,
            async       =>  ref_1pps,
            sync        =>  ref_1pps_sync
        );

    -- Generate a single-cycle version of the 1PPS signal
    U_edge_detector_pps : entity work.edge_detector
        generic map (
            EDGE_RISE       => '1',
            EDGE_FALL       => '0'
        )
        port map (
            clock           => vctcxo_clock,
            reset           => vctcxo_reset,
            sync_in         => ref_1pps_sync,
            pulse_out       => ref_1pps_pulse
        );

    -- Count number of VCTCXO clock cycles in the last 1 second
    U_pps_counter_1s : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_1s.count'length,
            PPS_PULSES      => 1
        )
        port map (
            sys_clock       => mm_clock,
            sys_reset       => mm_reset or mm_vctcxo_reset,
            sys_count       => pps_1s.count,
            sys_count_v     => pps_1s.count_v,
            vctcxo_clock    => vctcxo_clock,
            vctcxo_reset    => vctcxo_reset,
            vctcxo_pps      => ref_1pps_pulse
        );

    -- Count number of VCTCXO clock cycles in the last 10 seconds
    U_pps_counter_10s : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_10s.count'length,
            PPS_PULSES      => 10
        )
        port map (
            sys_clock       => mm_clock,
            sys_reset       => mm_reset or mm_vctcxo_reset,
            sys_count       => pps_10s.count,
            sys_count_v     => pps_10s.count_v,
            vctcxo_clock    => vctcxo_clock,
            vctcxo_reset    => vctcxo_reset,
            vctcxo_pps      => ref_1pps_pulse
        );

    -- Count number of VCTCXO clock cycles in the last 100 seconds
    U_pps_counter_100s : entity work.pps_counter
        generic map (
            COUNT_WIDTH     => pps_100s.count'length,
            PPS_PULSES      => 100
        )
        port map (
            sys_clock       => mm_clock,
            sys_reset       => mm_reset or mm_vctcxo_reset,
            sys_count       => pps_100s.count,
            sys_count_v     => pps_100s.count_v,
            vctcxo_clock    => vctcxo_clock,
            vctcxo_reset    => vctcxo_reset,
            vctcxo_pps      => ref_1pps_pulse
        );

    -- Interrupt Request
    int_req_proc : process( mm_clock )
        variable tmp_1s_err   : signed(PPS_1S_ERROR_TOL'range)   := (others => '0');
        variable tmp_10s_err  : signed(PPS_10S_ERROR_TOL'range)  := (others => '0');
        variable tmp_100s_err : signed(PPS_100S_ERROR_TOL'range) := (others => '0');
    begin
        if( rising_edge(mm_clock) ) then

            if( (pps_1s.count_v = '1') and (mm_pps_irq_enable = '1') ) then
                tmp_1s_err   := resize( (pps_1s.count - pps_1s.target), 32 );
                pps_1s.error <= tmp_1s_err;
                if( abs(tmp_1s_err) > PPS_1S_ERROR_TOL ) then
                    pps_1s.error_v <= '1';
                    mm_irq <= '1';
                end if;
            end if;

            if( (pps_10s.count_v = '1') and (mm_pps_irq_enable = '1') ) then
                tmp_10s_err   := resize( (pps_10s.count - pps_10s.target), 32 );
                pps_10s.error <= tmp_10s_err;
                if( abs(tmp_10s_err) > PPS_10S_ERROR_TOL ) then
                    pps_10s.error_v <= '1';
                    mm_irq <= '1';
                end if;
            end if;

            if( (pps_100s.count_v = '1') and (mm_pps_irq_enable = '1') ) then
                tmp_100s_err   := resize( (pps_100s.count - pps_100s.target), 32 );
                pps_100s.error <= tmp_100s_err;
                if( abs(tmp_100s_err) > PPS_100S_ERROR_TOL ) then
                    pps_100s.error_v <= '1';
                    mm_irq <= '1';
                end if;
            end if;

            if( mm_pps_irq_clear = '1' ) then
                -- Don't need to clear out the error count
                -- because we are invalidating it here.
                pps_1s.error_v   <= '0';
                pps_10s.error_v  <= '0';
                pps_100s.error_v <= '0';
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

                -- Control Register
                when CONTROL_ADDR =>
                    mm_rd_data <= mm_control_reg;

                -- PPS Error Status
                when PPS_ERR_STATUS =>
                    mm_rd_data(7 downto 3) <= (others => '0');
                    mm_rd_data(2)          <= pps_100s.error_v;
                    mm_rd_data(1)          <= pps_10s.error_v;
                    mm_rd_data(0)          <= pps_1s.error_v;

                -- 1 Second Count Error
                when PPS_ERR_1S_ADDR0 =>
                    mm_rd_data <= std_logic_vector(pps_1s.error(7 downto 0));

                when PPS_ERR_1S_ADDR1 =>
                    mm_rd_data <= std_logic_vector(pps_1s.error(15 downto 8));

                when PPS_ERR_1S_ADDR2 =>
                    mm_rd_data <= std_logic_vector(pps_1s.error(23 downto 16));

                when PPS_ERR_1S_ADDR3 =>
                    mm_rd_data <= std_logic_vector(pps_1s.error(31 downto 24));

                -- 10 Second Count Error
                when PPS_ERR_10S_ADDR0 =>
                    mm_rd_data <= std_logic_vector(pps_10s.error(7 downto 0));

                when PPS_ERR_10S_ADDR1 =>
                    mm_rd_data <= std_logic_vector(pps_10s.error(15 downto 8));

                when PPS_ERR_10S_ADDR2 =>
                    mm_rd_data <= std_logic_vector(pps_10s.error(23 downto 16));

                when PPS_ERR_10S_ADDR3 =>
                    mm_rd_data <= std_logic_vector(pps_10s.error(31 downto 24));

                -- 100 Second Count Error
                when PPS_ERR_100S_ADDR0 =>
                    mm_rd_data <= std_logic_vector(pps_100s.error(7 downto 0));

                when PPS_ERR_100S_ADDR1 =>
                    mm_rd_data <= std_logic_vector(pps_100s.error(15 downto 8));

                when PPS_ERR_100S_ADDR2 =>
                    mm_rd_data <= std_logic_vector(pps_100s.error(23 downto 16));

                when PPS_ERR_100S_ADDR3 =>
                    mm_rd_data <= std_logic_vector(pps_100s.error(31 downto 24));

                when others =>
                    null;

            end case;
        end if;
    end process;

    -- Avalon-MM Write Process
    mm_write_proc : process( mm_clock )
    begin
        if( rising_edge(mm_clock) ) then
            mm_pps_irq_clear <= '0';
            if( mm_wr_req = '1' ) then
                case to_integer(unsigned(mm_addr)) is
                    when CONTROL_ADDR =>
                        mm_control_reg <= mm_wr_data;
                    when others =>
                        null;
                end case;
            end if;
        end if;
    end process;

    -- Asynchronous reset, synchronous deassertion of reset
    U_reset_sync_vctcxo : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL     => '1',
            OUTPUT_LEVEL    => '1'
        )
        port map (
            clock           => vctcxo_clock,
            async           => mm_vctcxo_reset or mm_reset,
            sync            => vctcxo_reset
        );

    -- Asynchronous reset, synchronous deassertion of reset
    U_reset_sync_tune_ref : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL     => '1',
            OUTPUT_LEVEL    => '1'
        )
        port map (
            clock           => tune_ref,
            async           => mm_vctcxo_reset or mm_reset,
            sync            => tune_ref_reset
        );

    -- Tune Mode Updater: Keep requesting tune_mode updates
    tune_mode_updater_proc : process( tune_ref, tune_ref_reset )
    begin
        if( tune_ref_reset = '1' ) then
            tune_ref_mode_update_req <= '0';
            tune_ref_mode            <= DISABLED;
        elsif( rising_edge(tune_ref) ) then
            if( tune_ref_mode_update_ack = '1' ) then
                tune_ref_mode_update_req <= '0';
                tune_ref_mode            <= tune_ref_mode_hs;
            else
                tune_ref_mode_update_req <= '1';
            end if;
        end if;
    end process;

    -- Get the tune mode into the tune_ref clock domain
    U_handshake_tune_mode : entity work.handshake
        generic map (
            DATA_WIDTH        => 2
        )
        port map (
            source_reset      => mm_reset or mm_vctcxo_reset,
            source_clock      => mm_clock,
            source_data       => mm_tune_mode,
            dest_reset        => tune_ref_reset,
            dest_clock        => tune_ref,
            unpack(dest_data) => tune_ref_mode_hs,
            dest_req          => tune_ref_mode_update_req,
            dest_ack          => tune_ref_mode_update_ack
        );

end architecture;
