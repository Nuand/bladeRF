library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;

entity tan_table is
    generic(
        Q       :       positive := 12;
        INPUT_WIDTH :   positive := 16;
        OUTPUT_WIDTH:   positive := 16;
        STEPS   :       positive := 4;
        ANGLE   :       real := MATH_PI*(45.0/180.0)
    );
    port(
        reset   :       in std_logic;
        clock   :       in std_logic;

        phase   :       in signed(INPUT_WIDTH-1 downto 0);
        phase_valid :   in std_logic;

        y       :       out signed(OUTPUT_WIDTH-1 downto 0);
        y_valid :       out std_logic
    );
end entity;


architecture arch of tan_table is

    type tan_table_t is array (natural range <>) of signed(OUTPUT_WIDTH-1 downto 0);
    type tan_table_real_t is array (natural range <>) of real;

    function generate_tan_table(steps, qscale : positive) return tan_table_t is
        variable table : tan_table_t(0 to steps) := (others => (others => '0'));
    begin
        for i in table'range loop
            table(i) := to_signed(integer(round(real(2**qscale)* tan(ANGLE * real(i)/real(steps)))),table(i)'length);
        end loop;
        return table;
    end function;

    function generate_tan_table_real(steps : positive) return tan_table_real_t is
        variable table : tan_table_real_t(0 to steps) := (others => (0.0));
    begin
        for i in table'range loop
            table(i) :=tan(ANGLE * real(i)/real(steps));
        end loop;
        return table;
    end function;


    constant table : tan_table_t(0 to STEPS) := generate_tan_table(STEPS,Q);
    constant table_2 : tan_table_real_t(0 to STEPS) := generate_tan_table_real(STEPS);

begin

    find_y : process(clock, reset)
    begin
        if reset = '1' then
            --
            y_valid <= '0';
            y <= (others => '0');
        elsif rising_edge(clock) then
            y_valid <= '0';
            if phase_valid = '1' then
                if phase < 0 then
                    y <= table(to_integer(abs(phase)));
                else
                    y <= -table(to_integer(abs(phase)));
                end if;
                
                y_valid <= '1';
            end if;
            --
        end if;
    end process;

end architecture;
