-- Copyright (c) 2013 Nuand LLC
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

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

package cordic_p is

    -- Vectoring mode forces outputs.y to 0
    -- Rotation mode forces outputs.z to 0
    type cordic_mode_t is (CORDIC_ROTATION, CORDIC_VECTORING) ;

    -- TODO: Make this a generic package
    type cordic_xyz_t is record
        x       :   signed(15 downto 0) ;
        y       :   signed(15 downto 0) ;
        z       :   signed(15 downto 0) ;
        valid   :   std_logic ;
    end record ;

end package ; -- cordic_p

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;

library work ;
    use work.cordic_p.all ;

entity cordic is
  port (
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;
    mode        :   in  cordic_mode_t ;
    inputs      :   in  cordic_xyz_t ;
    outputs     :   out cordic_xyz_t
  ) ;
end entity ; -- cordic

architecture arch of cordic is

    -- TODO: Make this generic
    constant NUM_STAGES : natural := 12 ;

    -- TODO: Use the VHDL-2008 integer_vector
    type integer_array_t is array(natural range <>) of integer ;

    -- Each stage of the CORDIC is atan(2^(-i))
    function calculate_cordic_table return integer_array_t is
        variable rv : integer_array_t(0 to NUM_STAGES-1) := (others => 0) ;
    begin
        for i in 0 to rv'high loop
            rv(i) := integer(round((2.0**NUM_STAGES)*arctan(2.0**(-i))/MATH_PI)) ;
        end loop ;
        return rv ;
    end function ;

    -- Constant array
    constant K : integer_array_t := calculate_cordic_table ;

    -- The array for CORDIC stages
    type xyzs_t is array(0 to NUM_STAGES-1) of cordic_xyz_t ;

    -- ALl the XYZ's for the CORDIC stages
    signal xyzs : xyzs_t ;

begin

    rotate : process(clock, reset)
    begin
        if( reset = '1' ) then
            xyzs <= (others =>(x => (others =>'0'), y => (others =>'0'), z => (others =>'0'), valid => '0')) ;
        elsif( rising_edge( clock ) ) then
            -- First stage will rotate the vector to be within -pi/2 to pi/2
            xyzs(0).valid <= inputs.valid ;
            if( inputs.valid = '1' ) then
                case mode is
                    when CORDIC_ROTATION =>
                        -- Make sure we're only rotating -pi/2 to pi/2 and adjust accordingly
                        if( inputs.z > 2**(NUM_STAGES-1) ) then
                            xyzs(0).x <= -inputs.x ;
                            xyzs(0).y <= -inputs.y ;
                            xyzs(0).z <= inputs.z - 2**NUM_STAGES ;
                        elsif( inputs.z < -(2**(NUM_STAGES-1)) ) then
                            xyzs(0).x <= -inputs.x ;
                            xyzs(0).y <= -inputs.y ;
                            xyzs(0).z <= inputs.z + 2**NUM_STAGES ;
                        else
                            xyzs(0).x <= inputs.x ;
                            xyzs(0).y <= inputs.y ;
                            xyzs(0).z <= inputs.z ;
                        end if ;
                    when CORDIC_VECTORING =>
                        -- Make sure we're in the 1st or 4th quadrant only
                        if( inputs.x < 0 and inputs.y < 0 ) then
                            xyzs(0).x <= -inputs.x ;
                            xyzs(0).y <= -inputs.y ;
                            xyzs(0).z <= inputs.z - 2**(NUM_STAGES) ;
                        elsif( inputs.x < 0 ) then
                            xyzs(0).x <= -inputs.x ;
                            xyzs(0).y <= -inputs.y ;
                            xyzs(0).z <= inputs.z + 2**(NUM_STAGES) ;
                        else
                            xyzs(0).x <= inputs.x ;
                            xyzs(0).y <= inputs.y ;
                            xyzs(0).z <= inputs.z ;
                        end if ;
                end case ;

            end if ;

            -- Run through all the other stages
            for i in 0 to xyzs'high-1 loop
                xyzs(i+1).valid <= xyzs(i).valid ;
                if( xyzs(i).valid = '1' ) then
                    case mode is
                        when CORDIC_ROTATION =>
                            if( xyzs(i).z < 0 ) then
                                xyzs(i+1).x <= xyzs(i).x + shift_right(xyzs(i).y, i) ;
                                xyzs(i+1).y <= xyzs(i).y - shift_right(xyzs(i).x, i) ;
                                xyzs(i+1).z <= xyzs(i).z + K(i) ;
                            else
                                xyzs(i+1).x <= xyzs(i).x - shift_right(xyzs(i).y, i) ;
                                xyzs(i+1).y <= xyzs(i).y + shift_right(xyzs(i).x, i) ;
                                xyzs(i+1).z <= xyzs(i).z - K(i) ;
                            end if ;
                        when CORDIC_VECTORING =>
                            if( xyzs(i).y < 0 ) then
                                xyzs(i+1).x <= xyzs(i).x - shift_right(xyzs(i).y, i) ;
                                xyzs(i+1).y <= xyzs(i).y + shift_right(xyzs(i).x, i) ;
                                xyzs(i+1).z <= xyzs(i).z - K(i) ;
                            else
                                xyzs(i+1).x <= xyzs(i).x + shift_right(xyzs(i).y, i) ;
                                xyzs(i+1).y <= xyzs(i).y - shift_right(xyzs(i).x, i) ;
                                xyzs(i+1).z <= xyzs(i).z + K(i) ;
                            end if ;
                    end case ;
                end if ;
            end loop ;

            -- Output stage
            outputs <= xyzs(xyzs'high) ;
        end if ;
    end process ;

end architecture ; -- arch
