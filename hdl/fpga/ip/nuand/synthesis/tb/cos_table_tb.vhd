library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
	use ieee.math_real.all;
library std;
	use std.env.all;


entity cos_table_tb is
end entity;


architecture arch of cos_table_tb is
	
	signal clock : std_logic := '1';
	signal reset : std_logic := '1';

	constant Q_SCALE : positive := 12;
	constant SIGNAL_LENGTH : positive := 16;
	constant PHASE_STEPS_90 : positive := 1024;
	constant PHASE_STEPS : positive := PHASE_STEPS_90*4;

	signal phase_request : unsigned(SIGNAL_LENGTH-1 downto 0);
	signal phase_request_valid : std_logic := '1';

	signal cos_value : signed(SIGNAL_LENGTH-1 downto 0);
	signal cos_value_valid : std_logic;

	signal cos_ieee : signed(SIGNAL_LENGTH-1 downto 0);

	signal cos_error : signed(SIGNAL_LENGTH-1 downto 0);

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge( clock ) ;
        end loop ;
    end procedure ;

begin

	clock <= not clock after 1 ns;

	--
	U_cos_table : entity work.cos_table
		generic map(
			Q => Q_SCALE,
			DPHASE_STEPS_90 => PHASE_STEPS_90
		)
		port map(
			reset => reset,
			clock => clock,

			phase => phase_request,
			phase_valid => phase_request_valid,

			y => cos_value,
			y_valid => cos_value_valid
		);

	tb : process
		--
	begin 
		reset <= '1';
		nop(clock, 10);

		phase_request <= (others => '0');
		phase_request_valid <= '0';
		reset <= '0';
		nop(clock, 10);

		for i in 0 to PHASE_STEPS-1 loop
			wait until rising_edge(clock);
			phase_request <= to_unsigned(i, phase_request'length);
			if phase_request_valid = '1' then 
				cos_ieee <= to_signed(integer(round(real(2**Q_SCALE)*cos( (MATH_PI*2.0) * (real(i-1)/real(PHASE_STEPS))) )),cos_ieee'length);
			end if;
			phase_request_valid <= '1';
		end loop;

		phase_request_valid <= '0';
		nop(clock, 1000);
        report "-- End of Simulation --";
        stop(2) ;
	end process;

	error_check : process
	begin
		wait until rising_edge(clock);
		cos_error <= cos_ieee - cos_value;


	end process;



end architecture;

