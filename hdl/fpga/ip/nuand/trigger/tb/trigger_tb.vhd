library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
   
entity trigger_test is
end entity;

architecture arch of trigger_test is
    constant CLOCK_FREQUENCY    : REAL   := 40.0e6;
    constant CLOCK_PERIOD       : time      := 1.0 / CLOCK_FREQUENCY*1 sec;
    constant CLOCK_HALF_PERIOD  : time      := CLOCK_PERIOD / 2.0;
    
    signal clk                  : std_logic := '1';
    signal arm                  : std_logic := '0';
    signal trig              : std_logic := '1';
    signal sig                  : std_logic := '0';
    signal sigout               : std_logic := '0';
    
begin
    clk <= not clk after CLOCK_HALF_PERIOD;
    sig <= clk;
    
    trigger_dut : entity work.trigger
        generic map (
            DEFAULT_OUTPUT => '0'
        ) port map (
            armed => arm,
            trigger_in => trig,
            signal_in => sig,
            signal_out => sigout
        );
        
    tb : process
    begin
        wait for 100ns;
        arm <= '1';
        wait for 100ns;
        trig <= '0';
        wait for 100ns;
        arm <= '0';
        wait for 100ns;
        trig <= '1';
    end process;
end architecture;
