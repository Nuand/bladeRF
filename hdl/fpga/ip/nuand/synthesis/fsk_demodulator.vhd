library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

library work ;
    use work.cordic_p.all ;

entity fsk_demodulator is
  port (
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;

    in_real     :   in  signed(15 downto 0) ;
    in_imag     :   in  signed(15 downto 0) ;
    in_valid    :   in  std_logic ;

    out_ssd     :   buffer signed(15 downto 0) ;
    out_valid   :   out std_logic
  ) ;
end entity ; -- fsk_demodulator

architecture arch of fsk_demodulator is

    signal cordic_inputs    : cordic_xyz_t ;
    signal cordic_outputs   : cordic_xyz_t ;

    signal prev_z           : signed(15 downto 0) ;
    signal delta_z          : signed(15 downto 0) ;
    signal delta_z_valid    : std_logic ;
    signal unwrapped_z      : signed(15 downto 0) ;
    signal unwrapped_valid  : std_logic ;

begin

    cordic_inputs <= ( x => in_real, y => in_imag, z => (others =>'0'), valid => in_valid ) ;

    U_cordic : entity work.cordic
      port map (
        clock   => clock,
        reset   => reset,
        mode    => CORDIC_VECTORING,
        inputs  => cordic_inputs,
        outputs => cordic_outputs
      ) ;

    find_derivative : process(clock, reset)
    begin
        if( reset = '1' ) then
            prev_z <= (others =>'0') ;
            delta_z <= (others =>'0') ;
            delta_z_valid <= '0' ;
        elsif( rising_edge( clock ) ) then
            delta_z_valid <= cordic_outputs.valid ;
            if( cordic_outputs.valid = '1' ) then
                prev_z <= cordic_outputs.z ;
                delta_z <= cordic_outputs.z - prev_z ;
            end if ;
        end if ;
    end process ;

    unwrap : process(clock, reset)
    begin
        if( reset = '1' ) then
            unwrapped_z <= (others =>'0') ;
            unwrapped_valid <= '0' ;
        elsif( rising_edge( clock ) ) then
            unwrapped_valid <= delta_z_valid ;
            if( delta_z > 4096 ) then
                unwrapped_z <= delta_z - 8192 ;
            elsif( delta_z < -4096 ) then
                unwrapped_z <= delta_z + 8192 ;
            else
                unwrapped_z <= delta_z ;
            end if ;
        end if ;
    end process ;

    out_ssd <= out_ssd - shift_right(out_ssd,4) + unwrapped_z when rising_edge(clock) and unwrapped_valid = '1' ;
    out_valid <= unwrapped_valid when rising_edge(clock);

end architecture ; -- arch