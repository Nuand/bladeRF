library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;

library work ;
    use work.cordic_p.all ;

entity cordic_tb is
end entity ; -- cordic_tb

architecture arch of cordic_tb is

    signal clock    : std_logic   := '1' ;
    signal reset    : std_logic   := '1' ;
    signal inputs   : cordic_xyz_t  := ( x => (others =>'0'), y => (others =>'0'), z => (others =>'0'), valid => '0' );
    signal outputs  : cordic_xyz_t ;
    signal outputs2 : cordic_xyz_t ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge( clock ) ;
        end loop ;
    end procedure ; -- nop

begin

    -- Generate clock
    clock <= not clock after 1 ns ;

    -- Magnitude/phase to cos+isin
    U_cordic : entity work.cordic
      port map (
        clock       => clock,
        reset       => reset,
        mode        => CORDIC_ROTATION,
        inputs      => inputs,
        outputs     => outputs
      ) ;

    -- cos+isin to magnitude/phase
    U_cordic2 : entity work.cordic
      port map (
        clock       => clock,
        reset       => reset,
        mode        => CORDIC_VECTORING,
        inputs      => outputs,
        outputs     => outputs2
      ) ;

    tb : process
        variable ang    : integer := 0 ;
        variable dang   : integer := integer(round(0.01*4096.0)) ; 
    begin
        reset <= '1' ;
        nop( clock, 10 ) ;

        reset <= '0' ;
        nop( clock, 10 ) ;

        ang := 0 ;
        inputs.x <= to_signed( integer(round(4096.0/1.66)), inputs.x'length ) ;
        inputs.y <= to_signed(    0, inputs.y'length ) ;
        for i in 0 to 10000 loop
            inputs.z <= to_signed( ang, inputs.z'length ) ;
            inputs.valid <= '1' ;
            wait until rising_edge( clock ) ;
            inputs.valid <= '0' ;
            nop( clock, 20 ) ;
            ang := ang + dang ;
            if( ang > 4096 ) then
                ang := ang - 8192 ;
            elsif( ang < -4096 ) then
                ang := ang + 8192 ;
            end if ;

        end loop ;

        report "-- End of Simulation --" severity failure ;

    end process ;

end architecture ; -- arch