-- Copyright (c) 2026 Distributed Spectrum
--
-- Testbench for ctrl_out_xfer.
--
-- The property under test is that the destination never observes a byte the
-- source did not hold as a whole. The stimulus deliberately mimics what eight
-- independent per-bit synchronizers do to a multi-bit transition: it passes
-- through an intermediate mixture of the old and new values for a single source
-- cycle. Only the whole values may ever appear downstream.

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity ctrl_out_xfer_tb is
end entity ;

architecture arch of ctrl_out_xfer_tb is

    -- Deliberately unrelated periods and a fast destination, as in the real
    -- design (sys_clock 80 MHz source, rx_clock 125 MHz destination).
    constant SRC_HALF_PERIOD :   time := 1 sec / (80.0e6 * 2.0) ;
    constant DST_HALF_PERIOD :   time := 1 sec / (125.0e6 * 2.0) ;

    -- Gain indices the source walks between, and the torn mixture of the two
    -- that must never survive the crossing.
    constant VALUE_A         :   std_logic_vector(7 downto 0) := x"28" ;  -- 40
    constant VALUE_B         :   std_logic_vector(7 downto 0) := x"34" ;  -- 52
    constant VALUE_TORN      :   std_logic_vector(7 downto 0) := x"2C" ;  -- mixture

    signal src_clock         :   std_logic := '1' ;
    signal src_reset         :   std_logic := '1' ;
    signal src_data          :   std_logic_vector(7 downto 0) := VALUE_A ;

    signal dst_clock         :   std_logic := '1' ;
    signal dst_reset         :   std_logic := '1' ;
    signal dst_data          :   std_logic_vector(7 downto 0) ;
    signal dst_valid         :   std_logic ;

    signal transitions       :   natural := 0 ;
    signal saw_a             :   natural := 0 ;
    signal saw_b             :   natural := 0 ;
    signal done              :   boolean := false ;

    procedure nop( signal clock : in std_logic ; count : natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

begin

    src_clock <= not src_clock after SRC_HALF_PERIOD ;
    dst_clock <= not dst_clock after DST_HALF_PERIOD ;

    U_dut : entity work.ctrl_out_xfer
        port map (
            src_clock   =>  src_clock,
            src_reset   =>  src_reset,
            src_data    =>  src_data,

            dst_clock   =>  dst_clock,
            dst_reset   =>  dst_reset,
            dst_data    =>  dst_data,
            dst_valid   =>  dst_valid
        ) ;

    -- Walk between the two values, each transition passing through the torn
    -- mixture for exactly one source cycle.
    stimulus : process
    begin
        src_reset <= '1' ;
        src_data  <= VALUE_A ;
        nop( src_clock, 10 ) ;
        src_reset <= '0' ;
        nop( src_clock, 20 ) ;

        for i in 1 to 40 loop
            -- A -> torn -> B
            src_data <= VALUE_TORN ;
            nop( src_clock, 1 ) ;
            src_data <= VALUE_B ;
            nop( src_clock, 25 ) ;

            -- B -> torn -> A
            src_data <= VALUE_TORN ;
            nop( src_clock, 1 ) ;
            src_data <= VALUE_A ;
            nop( src_clock, 25 ) ;

            transitions <= transitions + 2 ;
        end loop ;

        done <= true ;
        wait ;
    end process ;

    dst_tb : process
    begin
        dst_reset <= '1' ;
        nop( dst_clock, 10 ) ;
        dst_reset <= '0' ;
        wait ;
    end process ;

    -- The whole point of the block: a torn value must never reach here.
    check : process( dst_clock )
    begin
        if( rising_edge(dst_clock) and dst_valid = '1' ) then
            assert( dst_data = VALUE_A or dst_data = VALUE_B )
                report "ctrl_out_xfer presented a value the source never held " &
                       "as a whole: " & to_hstring(dst_data)
                severity failure ;

            if( dst_data = VALUE_A ) then
                saw_a <= saw_a + 1 ;
            else
                saw_b <= saw_b + 1 ;
            end if ;
        end if ;
    end process ;

    -- Guard against the check passing only because nothing ever arrived, or
    -- because the destination latched one value and then stopped refreshing.
    finish : process
    begin
        wait until done ;
        nop( dst_clock, 200 ) ;

        assert( saw_a > 0 )
            report "destination never observed the first source value"
            severity failure ;
        assert( saw_b > 0 )
            report "destination never observed the second source value, so the " &
                   "transfer is not refreshing"
            severity failure ;

        report "-- ctrl_out_xfer: " & integer'image(transitions) &
               " transitions, saw A " & integer'image(saw_a) &
               " / B " & integer'image(saw_b) & " times, no torn values --" ;
        report "-- End of Simulation --" ;
        std.env.stop(0) ;
        wait ;
    end process ;

end architecture ;
