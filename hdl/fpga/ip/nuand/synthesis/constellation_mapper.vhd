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
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

package constellation_mapper_p is

    -- All possible modulations
    type constellation_mapper_modulation_t is (
        APSK_16,
        APSK_32,
        MSK,
        PSK_2,
        PSK_4,
        PSK_8,
        PSK_8_GRAY,
        PSK_16,
        QAM_16,
        QAM_32,
        QAM_64,
        QAM_128,
        QAM_256,
        QAM_512,
        QAM_1024,
        QAM_2048,
        QAM_4096,
        VSB_8,
        MAX_MODULATION
    ) ;

    type complex_fixed_t is record
        re : signed(15 downto 0);
        im : signed(15 downto 0);
    end record;

    type complex_fixed_array_t is array(natural range <>) of complex_fixed_t;

    type real_array_t is array(natural range <>) of real ;
    type complex_array_t is array(natural range <>) of complex ;

    -- Array of modulations
    type constellation_mapper_modulation_array_t is array(natural range <>) of constellation_mapper_modulation_t ;

    -- Inputs to the mapper
    type constellation_mapper_inputs_t is record
        bits        :   std_logic_vector(15 downto 0) ;
        modulation  :   constellation_mapper_modulation_t ;
        valid       :   std_logic ;
    end record ;

    -- Outputs of the mapper
    type constellation_mapper_outputs_t is record
        symbol      :   complex_fixed_t ;
        valid       :   std_logic ;
    end record ;

end package ; -- constellation_mapper_p

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

library work ;
    use work.constellation_mapper_p.all ;

entity constellation_mapper is
  generic (
    SCALE_FACTOR : real := 4096.0;
    MODULATIONS :       constellation_mapper_modulation_array_t := ( MSK, PSK_2, QAM_16, VSB_8 )
  ) ;
  port (
    clock       :   in  std_logic ;
    inputs      :   in  constellation_mapper_inputs_t ;
    outputs     :   out constellation_mapper_outputs_t
  ) ;
end entity ; -- constellation_mapper

architecture arch of constellation_mapper is

    type modulation_info_t is record
        index   :   natural ;
        mask    :   std_logic_vector(15 downto 0) ;
    end record ;

    -- NOTE: If QAM_4096 is ever not the end, this needs to change
    type modulation_index_t is array(0 to constellation_mapper_modulation_t'pos(MAX_MODULATION)-1) of modulation_info_t ;

    -- Get the number of entries of each of the supported modulations
    function size( x : constellation_mapper_modulation_t ) return natural is
        variable rv : natural := 0;
    begin
        case x is
            when APSK_16    => rv := 16;
            when APSK_32    => rv := 32;
            when MSK        => rv := 2 ;
            when PSK_2      => rv := 2 ;
            when PSK_4      => rv := 4 ;
            when PSK_8      => rv := 8 ;
            when PSK_8_GRAY => rv := 8 ;
            when PSK_16     => rv := 16 ;
            when QAM_16     => rv := 16 ;
            when QAM_32     => rv := 32 ;
            when QAM_64     => rv := 64 ;
            when QAM_128    => rv := 128 ;
            when QAM_256    => rv := 256 ;
            when QAM_512    => rv := 512 ;
            when QAM_1024   => rv := 1024 ;
            when QAM_2048   => rv := 2048 ;
            when QAM_4096   => rv := 4096 ;
            when VSB_8      => rv := 8;
            when MAX_MODULATION => rv := 0;
        end case ;
        return rv ;
    end function ;

    -- 16-APSK Modulation points
    function constellation_apsk_16 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 15) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 32-APSK Modulation Points
    function constellation_apsk_32 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 31) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- MSK Modulation Points
    function constellation_msk return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 1) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- BPSK Modulation Points
    function constellation_psk_2 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 1) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- QPSK Modulation Points
    function constellation_psk_4 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 3) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 8-PSK Modulation Points
    function constellation_psk_8 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 7) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 8-PSK Offset Modulation Points
    function constellation_psk_8_gray return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 7) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 16-PSK Modulation Points
    function constellation_psk_16 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 15) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 16-QAM Modulation Points
    function constellation_qam_16 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 15) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 32-QAM Modulation Points
    function constellation_qam_32 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 31) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 64-QAM Modulation Points
    function constellation_qam_64 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 63) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 128-QAM Modulation Points
    function constellation_qam_128 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 127) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 256-QAM Modulation Points
    function constellation_qam_256 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 255) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 512-QAM Modulation Points
    function constellation_qam_512 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 511) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 1024-QAM Modulation Points
    function constellation_qam_1024 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 1023) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 2048-QAM Modulation Points
    function constellation_qam_2048 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 2047) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;

    -- 4096-QAM Modulation Points
    function constellation_qam_4096 return complex_fixed_array_t is
        variable rv : complex_fixed_array_t(0 to 4096) := (others =>(to_signed(0,16),to_signed(0,16))) ;
    begin
        return rv ;
    end function ;


    -- 8- VSB
    function constellation_vsb_8 return complex_fixed_array_t is
        -- (-7.0,0.0)(-5.0,0.0),(-3.0,0.0),(-1.0,0.0),(1.0,0.0),(3.0,0.0)(5.0,0.0),(7.0,0.0)
        variable rv : complex_fixed_array_t(0 to 7) := (
            (to_signed(natural(-7.0/7.0 * SCALE_FACTOR),16), to_signed(0,16)),
            (to_signed(natural(-5.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(-3.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(-1.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(1.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(3.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(5.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)),
            (to_signed(natural(7.0/7.0 * SCALE_FACTOR),16),to_signed(0,16)));
    begin
        return rv;
    end function;

    -- Get each of the constellation arrays
    function constellation( x : constellation_mapper_modulation_t ) return complex_fixed_array_t is
    begin
        case x is
            when APSK_16    => return constellation_apsk_16 ;
            when APSK_32    => return constellation_apsk_32 ;
            when MSK        => return constellation_msk ;
            when PSK_2      => return constellation_psk_2 ;
            when PSK_4      => return constellation_psk_4 ;
            when PSK_8      => return constellation_psk_8 ;
            when PSK_8_GRAY => return constellation_psk_8_gray ;
            when PSK_16     => return constellation_psk_16 ;
            when QAM_16     => return constellation_qam_16 ;
            when QAM_32     => return constellation_qam_32 ;
            when QAM_64     => return constellation_qam_64 ;
            when QAM_128    => return constellation_qam_128 ;
            when QAM_256    => return constellation_qam_256 ;
            when QAM_512    => return constellation_qam_512 ;
            when QAM_1024   => return constellation_qam_1024 ;
            when QAM_2048   => return constellation_qam_2048 ;
            when QAM_4096   => return constellation_qam_4096 ;
            when VSB_8      => return constellation_vsb_8 ;
            when MAX_MODULATION => return constellation_msk;
        end case ;
    end function ;

    -- Calculate the base indicies for each modulation type
    function calculate_indices( x : constellation_mapper_modulation_array_t ) return modulation_index_t is
        variable index  : natural                       := 0;
        variable rv : modulation_index_t            := (others =>(0, x"0000")) ;
    begin
        for i in x'range loop
            rv(constellation_mapper_modulation_t'pos(x(i))).index := index ;
            rv(constellation_mapper_modulation_t'pos(x(i))).mask := std_logic_vector(to_unsigned(size(x(i))-1,rv(constellation_mapper_modulation_t'pos(x(i))).mask'length)) ;
            index := index + size(x(i)) ;
        end loop ;
        return rv ;
    end function ;

    -- Take in the array and create the modulations recursively
    function generate_modulation_table( x : constellation_mapper_modulation_array_t ) return complex_fixed_array_t is
    begin
        report "x'low is " & integer'image(x'low) & " x'high is "  &integer'image(x'high);
        if (x'low + 1 <= x'high) then
            return constellation(x(x'low)) & generate_modulation_table(x(x'low+1 to x'high)) ;
        else
            return constellation(x(x'low));
        end if;
    end function ;

    -- Constant values generated
    constant modulation_index : modulation_index_t  := calculate_indices(MODULATIONS) ;
    constant modulation_table : complex_fixed_array_t     := generate_modulation_table(MODULATIONS) ;

    -- Actual signals!
    signal base     :   modulation_info_t ;
    signal symbol   :   complex_fixed_t ;

begin

    -- Get the inputs and base address
    base <= modulation_index(constellation_mapper_modulation_t'pos(inputs.modulation)) ;

    -- Map it to the modulation table
    symbol <= modulation_table(base.index + to_integer(unsigned(inputs.bits and base.mask))) ;

    -- Register the outputs
    outputs.symbol <= symbol when rising_edge( clock ) and inputs.valid = '1' ;
    outputs.valid <= inputs.valid when rising_edge( clock ) ;

end architecture ; -- arch
