library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;

library work;
    use work.constellation_mapper_p.all;

entity fir_filter is
    generic (
        INPUT_WIDTH : positive := 16;
        OUTPUT_WIDTH : positive := 16;

        CPS : positive := 1;
        Q : positive := 12;

        H : real_array_t := (1.0, 0.0 ,1.0);

        ACCUM_SCALE : positive := 32;
        OUTPUT_SHIFT : positive := 12

    );
    port(
        clock   :   in std_logic;
        reset   :   in std_logic;

        --input signal
        in_sample   :   in signed(INPUT_WIDTH-1 downto 0);
        in_valid    :   in std_logic;

        --output signal
        out_sample  :   out signed(OUTPUT_WIDTH-1 downto 0);
        out_valid   :   out std_logic
    );

end entity;

architecture systolic of fir_filter is

    function mult (a,b : signed; Q : positive) return signed is
    begin
        return resize(shift_right(a*b,Q),a'length);
    end function;

    --integer(ceil(log2( real(H'length*(2**(INPUT_WIDTH-1))) )))
    type accum_t is array( natural range <>) of signed( ACCUM_SCALE-1 downto 0);
    signal accum : accum_t(H'range);


    type coeff_t is array( natural range <>) of signed( (INPUT_WIDTH-1 ) downto 0);
    signal coeff : coeff_t(H'range);

    function scale_coeffecients (h : real_array_t; q : positive) return coeff_t is
        variable retval : coeff_t(h'range);
    begin
        for i in h'range loop
            retval(i) := to_signed( integer(round(h(i) * real(2**q))), OUTPUT_WIDTH);
        end loop;
        return retval;
    end function;

    constant COEF : coeff_t(H'range) :=  scale_coeffecients(H,Q);

begin

    mac : process(clock, reset)
    begin
        if reset = '1' then
            accum <= (others => (others => '0'));
            out_valid <= '0';
            out_sample <= (others =>'0');

        elsif rising_edge(clock) then
            --
            out_valid <= '0';
            if in_valid = '1' then

                for i in accum'range loop
                    if i = accum'high then
                        accum(i) <= resize(COEF(i)*in_sample, accum(i)'length);
                    else
                        accum(i) <= accum(i+1) + resize(COEF(i)*in_sample, accum(i)'length);
                    end if;
                end loop;

                out_valid <= '1';
                out_sample <= resize(shift_right(accum(0),OUTPUT_SHIFT),out_sample'length);
            end if;
        end if;

    end process;

end architecture;
