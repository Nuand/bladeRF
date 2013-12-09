library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;

library std;
    use std.env.all;

entity iq_correction_tb is
end entity;

architecture arch of iq_correction_tb is

    signal clock : std_logic := '1';
    signal reset : std_logic := '1';

    procedure nop( signal clock : in std_logic; count : in natural) is 
    begin
        for i in 1 to count loop
            wait until rising_edge(clock);
        end loop;
    end procedure;

    constant SIGNAL_LENGTH : natural := 1000;
    constant F_SAMP : real := 1.0e7;
    constant F_TONE : real := 100.0e3;
    constant PHASE_STEP : real := MATH_2_PI*(F_TONE/F_SAMP);

    constant Q_SCALE : positive := 12;
    constant DC_WIDTH : positive := 16;

    --error constants
    constant PHASE_OFFSET : real := (10.0/180.0);
    constant DC_OFFSET_REAL : real := 0.0;
    constant DC_OFFSET_IMAG : real := 0.0;
    constant GAIN_OFFSET : real := 1.0;

    signal PHASE_OFFSET_SIGNED :  signed(15 downto 0) := to_signed(integer(round(real(2**Q_SCALE) * PHASE_OFFSET)),DC_WIDTH);
    constant DC_OFFSET_REAL_SIGNED :  signed(15 downto 0) := to_signed(integer(round(real(2**Q_SCALE) * DC_OFFSET_REAL)),DC_WIDTH);
    constant DC_OFFSET_IMAG_SIGNED :  signed(15 downto 0) :=to_signed(integer(round(real(2**Q_SCALE) * DC_OFFSET_IMAG)),DC_WIDTH);
    constant GAIN_OFFSET_SIGNED :  signed(15 downto 0) := to_signed(integer(round(real(2**Q_SCALE) * GAIN_OFFSET)),DC_WIDTH);

    signal signal_real : signed(15 downto 0);
    signal signal_imag : signed(15 downto 0);
    signal signal_valid : std_logic;

    signal correction_valid : std_logic;


    signal corrected_real : signed(15 downto 0);
    signal corrected_imag : signed(15 downto 0);
    signal corrected_valid : std_logic;


    signal signal_ref_real : signed(15 downto 0);
    signal signal_ref_imag : signed(15 downto 0);


    signal error_real : signed(15 downto 0);
    signal error_imag : signed(15 downto 0);

    signal signal_real_float : real;
    signal signal_imag_float : real;

begin

    clock <= not clock after 1 ns;

    U_iq_correction : entity work.iq_correction(tx)
        generic map(
            INPUT_WIDTH => 16
        )
        port map(
            reset => reset,
            clock => clock,

            in_real => signal_real,
            in_imag => signal_imag,
            in_valid => signal_valid,

            out_real => corrected_real,
            out_imag => corrected_imag,
            out_valid => corrected_valid,

            dc_real => DC_OFFSET_REAL_SIGNED,
            dc_imag => DC_OFFSET_IMAG_SIGNED,
            gain => GAIN_OFFSET_SIGNED,
            phase => PHASE_OFFSET_SIGNED,

            correction_valid => correction_valid
        );

    tb : process
    begin 
        reset <= '1';
        signal_real <= (others => '0');
        signal_imag<= (others => '0');

        signal_real_float <= 0.0;
        signal_imag_float <= 0.0;
        correction_valid <= '0';
        signal_valid <= '0';

        nop(clock,10);
        correction_valid <= '1';
        reset <= '0';
        nop(clock,10);

        for i in 0 to SIGNAL_LENGTH loop
            wait until rising_edge(clock);

            signal_real <= to_signed(integer(round(real(3635)*(cos( PHASE_STEP*real(i)  ) + DC_OFFSET_REAL))),signal_real'length);
            signal_imag <= to_signed(integer(round(real(3635)*GAIN_OFFSET*(sin( PHASE_STEP*real(i) + PHASE_OFFSET*MATH_PI ) + DC_OFFSET_IMAG))),signal_imag'length);
            

            signal_real_float <= cos( PHASE_STEP*real(i)  ) + DC_OFFSET_REAL;
            signal_imag_float <= GAIN_OFFSET*sin( PHASE_STEP*real(i) + PHASE_OFFSET*MATH_PI ) + DC_OFFSET_IMAG;

            signal_valid <= '1';
            wait until rising_edge(clock);
            signal_valid <= '0';
        end loop;


        nop(clock, 100);
        report "-- End of Simulation --";
        stop(2) ;
    end process;

    compare : process(clock,reset)
        variable i : integer;
    begin
        if reset = '1' then
            --
            i := 0;
            signal_ref_real <= (others => '0');
            signal_ref_imag <= (others => '0');
            error_real <= (others => '0');
            error_imag <= (others => '0');
        elsif rising_edge(clock) then
            signal_ref_real <= to_signed(integer(round(real(3592)*(cos( PHASE_STEP*real(i)   + PHASE_OFFSET*MATH_PI/2.0 ) ))),signal_real'length);
            signal_ref_imag <= to_signed(integer(round(real(3592)*(sin( PHASE_STEP*real(i) +  PHASE_OFFSET*MATH_PI/2.0 ) ))),signal_imag'length);
            
            if corrected_valid = '1' then
                -- calc error
                error_real <= signal_ref_real - corrected_real;
                error_imag <= signal_ref_imag - corrected_imag;
                i := i + 1;
            end if;
        end if;
    end process;  
           


end architecture;