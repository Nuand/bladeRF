-- Copyright (c) 2017 Nuand LLC
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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

package bladerf_p is

    -- ========================================================================
    -- CONSTANTS
    -- ========================================================================



    -- ========================================================================
    -- TYPEDEFS
    -- ========================================================================

    type sky13374_397lf_t is (
        DISABLED,
        RF1_TO_RF2,
        RF1_TO_RF3,
        UNDEFINED
    );


    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS
    -- ========================================================================


    -- ========================================================================
    -- PACK FUNCTIONS -- pack a human-readable record/type into bits
    -- ========================================================================

    function pack( x : sky13374_397lf_t ) return std_logic_vector;


    -- ========================================================================
    -- UNPACK FUNCTIONS -- unpack bits into a human-readable record/type
    -- ========================================================================

    function unpack( x : std_logic_vector(1 downto 0) ) return sky13374_397lf_t;

end package;

package body bladerf_p is

    -- ========================================================================
    -- PACK FUNCTIONS
    -- ========================================================================

    function pack( x : sky13374_397lf_t ) return std_logic_vector is
        variable rv : unsigned(2 downto 1) := (others => '0');
    begin
        case x is
            when DISABLED   => rv := "00";
            when RF1_TO_RF2 => rv := "01";
            when RF1_TO_RF3 => rv := "10";
            when others     => rv := "00";
        end case;
        return std_logic_vector(rv);
    end function;


    -- ========================================================================
    -- UNPACK FUNCTIONS
    -- ========================================================================

    function unpack( x : std_logic_vector ) return sky13374_397lf_t is
        variable rv : sky13374_397lf_t;
    begin
        case x is
            when "00"   => rv := DISABLED;
            when "01"   => rv := RF1_TO_RF2;
            when "10"   => rv := RF1_TO_RF3;
            when others => rv := UNDEFINED;
        end case;
        return rv;
    end function;

end package body;
