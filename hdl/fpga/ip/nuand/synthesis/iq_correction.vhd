library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;



entity iq_correction is
    generic(
        INPUT_WIDTH : positive := 16;
        OUTPUT_WIDTH: positive := 16;
        DC_WIDTH    : positive := 16;
        GAIN_WIDTH  : positive := 16;
        PHASE_WIDTH : positive := 16;

        Q           : positive := 12

        );

    port(
        clock       :   in std_logic;
        reset       :   in std_logic;

        --input signal
        in_real     :   in signed(INPUT_WIDTH-1 downto 0);
        in_imag     :   in signed(INPUT_WIDTH-1 downto 0);
        in_valid    :   in std_logic;

        --output signal
        out_real    :   out signed(OUTPUT_WIDTH-1 downto 0);
        out_imag    :   out signed(OUTPUT_WIDTh-1 downto 0);
        out_valid   :   out std_logic;

        --correction signals
        dc_real     :   in signed(DC_WIDTH-1 downto 0);
        dc_imag     :   in signed(DC_WIDTH-1 downto 0);

        gain :   in signed(GAIN_WIDTH-1 downto 0);
        phase :   in signed(PHASE_WIDTH-1 downto 0);

        correction_valid:   in std_logic
    );
end entity;

architecture rx of iq_correction is

    signal dc_balanced_real : signed(INPUT_WIDTH-1 downto 0);
    signal dc_balanced_imag : signed(INPUT_WIDTH-1 downto 0);
    signal dc_balanced_valid : std_logic;

    signal gain_corrected_real : signed(INPUT_WIDTH-1 downto 0);
    signal gain_corrected_imag : signed(INPUT_WIDTH-1 downto 0);
    signal gain_corrected_valid : std_logic;

    signal phase_corrected_real : signed(OUTPUT_WIDTH-1 downto 0);
    signal phase_corrected_imag : signed(OUTPUT_WIDTH-1 downto 0);
    signal phase_corrected_valid : std_logic;

    signal phase_correction : signed(OUTPUT_WIDTH-1 downto 0);


begin
    --
    U_correction_lookup : entity work.tan_table
    generic map(
            STEPS => 2**Q,
            ANGLE => MATH_PI*(10.0/180.0)
    )
    port map
    (
        clock => clock,
        reset => reset,

        phase => phase,
        phase_valid => correction_valid,

        y => phase_correction,
        y_valid => open
    );

    --apply DC correction to real and imag
    apply_dc : process(clock, reset)
    begin 
        if reset = '1' then
            --
            dc_balanced_real <= (others => '0');
            dc_balanced_imag <= (others => '0');
            dc_balanced_valid <= '0';
        elsif rising_edge(clock) then
            dc_balanced_valid <= '0';
            if in_valid = '1' then
                dc_balanced_real <= in_real - dc_real;
                dc_balanced_imag <= in_imag - dc_imag;
                dc_balanced_valid <= '1';
            end if;
        end if;
    end process;

    --apply gain correction to real
    apply_gain : process(clock, reset)
    begin
        if reset = '1' then
            gain_corrected_real <= (others => '0');
            gain_corrected_imag <= (others => '0');
            gain_corrected_valid <= '0';
        elsif rising_edge(clock) then
            gain_corrected_valid <= '0';
            if dc_balanced_valid = '1' then
                gain_corrected_real <= resize(shift_right(dc_balanced_real*gain,Q),gain_corrected_real'length);
                gain_corrected_imag <= dc_balanced_imag;
                gain_corrected_valid <= '1';
            end if;
        end if;
    end process;

    --apply phase correction to imag
    apply_phase : process(clock, reset)
        variable register_offset_real : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_offset_imag : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_real : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_imag : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_valid : std_logic;

    begin
        if reset = '1' then
            phase_corrected_real <= (others => '0');
            phase_corrected_imag <= (others => '0');
            phase_corrected_valid <= '0';
            register_offset_real :=  (others => '0');
            register_offset_imag :=  (others => '0');
            register_real :=  (others => '0');
            register_imag :=  (others => '0');
            register_valid := '0';
        elsif rising_edge(clock) then
            phase_corrected_valid <= '0';
            if register_valid = '1' then
                phase_corrected_real <= register_real + register_offset_real;
                phase_corrected_imag <= register_imag + register_offset_imag;
                phase_corrected_valid <= '1';
            end if;

            register_valid := '0';
            if gain_corrected_valid = '1' then
                register_offset_real := resize(shift_right(gain_corrected_imag*phase_correction,Q),phase_corrected_real'length);
                register_offset_imag := resize(shift_right(gain_corrected_real*phase_correction,Q),phase_corrected_imag'length);
                register_real := gain_corrected_real;
                register_imag := gain_corrected_imag;
                register_valid := '1';
            end if;
        end if;
    end process;

    out_real <= phase_corrected_real;
    out_imag <= phase_corrected_imag;
    out_valid <= phase_Corrected_valid;

end architecture;



architecture tx of iq_correction is

    signal dc_balanced_real : signed(INPUT_WIDTH-1 downto 0);
    signal dc_balanced_imag : signed(INPUT_WIDTH-1 downto 0);
    signal dc_balanced_valid : std_logic;

    signal gain_corrected_real : signed(INPUT_WIDTH-1 downto 0);
    signal gain_corrected_imag : signed(INPUT_WIDTH-1 downto 0);
    signal gain_corrected_valid : std_logic;

    signal phase_corrected_real : signed(OUTPUT_WIDTH-1 downto 0);
    signal phase_corrected_imag : signed(OUTPUT_WIDTH-1 downto 0);
    signal phase_corrected_valid : std_logic;

    signal phase_correction : signed(OUTPUT_WIDTH-1 downto 0);

begin

    --apply gain correction to real
    apply_gain : process(clock, reset)
    begin
        if reset = '1' then
            gain_corrected_real <= (others => '0');
            gain_corrected_imag <= (others => '0');
            gain_corrected_valid <= '0';
        elsif rising_edge(clock) then
            gain_corrected_valid <= '0';
            if in_valid = '1' then
                gain_corrected_real <= resize(shift_right(in_real*gain,Q),gain_corrected_real'length);
                gain_corrected_imag <= in_imag;
                gain_corrected_valid <= '1';
            end if;
        end if;
    end process;


    --
    U_phase_correction_lookup : entity work.tan_table
    generic map(
            STEPS => 2**Q,
            ANGLE => MATH_PI*(10.0/180.0)
    )
    port map
    (
        clock => clock,
        reset => reset,

        phase => phase,
        phase_valid => correction_valid,

        y => phase_correction,
        y_valid => open
    );

    --apply phase correction to imag
    apply_phase : process(clock, reset)
        variable register_offset_real : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_offset_imag : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_real : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_imag : signed(OUTPUT_WIDTH-1 downto 0);
        variable register_valid : std_logic;

    begin
        if reset = '1' then
            phase_corrected_real <= (others => '0');
            phase_corrected_imag <= (others => '0');
            phase_corrected_valid <= '0';
            register_offset_real :=  (others => '0');
            register_offset_imag :=  (others => '0');
            register_real :=  (others => '0');
            register_imag :=  (others => '0');
            register_valid := '0';
        elsif rising_edge(clock) then
            phase_corrected_valid <= '0';
            if register_valid = '1' then
                phase_corrected_real <= register_real + register_offset_real;
                phase_corrected_imag <= register_imag + register_offset_imag;
                phase_corrected_valid <= '1';
            end if;

            register_valid := '0';
            if gain_corrected_valid = '1' then
                register_offset_real := resize(shift_right(gain_corrected_imag*phase_correction,Q),phase_corrected_real'length);
                register_offset_imag := resize(shift_right(gain_corrected_real*phase_correction,Q),phase_corrected_imag'length);
                register_real := gain_corrected_real;
                register_imag := gain_corrected_imag;
                register_valid := '1';
            end if;
        end if;
    end process;



    --apply DC correction to real and imag
    apply_dc : process(clock, reset)
    begin 
        if reset = '1' then
            --
            dc_balanced_real <= (others => '0');
            dc_balanced_imag <= (others => '0');
            dc_balanced_valid <= '0';
        elsif rising_edge(clock) then
            dc_balanced_valid <= '0';
            if phase_corrected_valid = '1' then
                dc_balanced_real <= phase_corrected_real - dc_real;
                dc_balanced_imag <= phase_corrected_imag - dc_imag;
                dc_balanced_valid <= '1';
            end if;
        end if;
    end process;

    out_real <= dc_balanced_real;
    out_imag <= dc_balanced_imag;
    out_valid <= dc_balanced_valid;

end architecture;
