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
    use ieee.math_real.all;

package common_dcfifo_p is

    function compute_rdusedw_high( numwords      : natural;
                                   width         : natural;
                                   width_r       : natural;
                                   add_usedw_msb : string
                                   ) return natural;

    function compute_wrusedw_high( numwords      : natural;
                                   add_usedw_msb : string
                                   ) return natural;

end package;

package body common_dcfifo_p is

    function compute_rdusedw_high( numwords      : natural;
                                   width         : natural;
                                   width_r       : natural;
                                   add_usedw_msb : string ) return natural is
        variable x  : real    := 0.0;
        variable y  : real    := 0.0;
        variable rv : natural := 0;
    begin
        x := real(numwords);

        if( width > width_r ) then
            y  := real(width) / real(width_r);
            rv := integer(ceil(log2(x) + log2(y))) - 1;
        else
            -- Less or equal. If equal, log2(y) is 0 and the term goes away.
            y  := real(width_r) / real(width);
            rv := integer(ceil(log2(x) - log2(y))) - 1;
        end if;

        if( add_usedw_msb = "on" or add_usedw_msb = "oN" or
            add_usedw_msb = "On" or add_usedw_msb = "ON" ) then
            rv := rv + 1;
        end if;

        return rv;
    end function;

    function compute_wrusedw_high( numwords      : natural;
                                   add_usedw_msb : string ) return natural is
        variable rv : natural := 0;
    begin
        rv := integer(ceil(log2(real(numwords)))) - 1;

        if( add_usedw_msb = "on" or add_usedw_msb = "oN" or
            add_usedw_msb = "On" or add_usedw_msb = "ON" ) then
            rv := rv + 1;
        end if;

        return rv;
    end function;

end package body;

library ieee;
    use ieee.std_logic_1164.all;

library altera_mf;
    use altera_mf.altera_mf_components.all;

library work;
    use work.common_dcfifo_p.all;

entity common_dcfifo is
    generic (
        ADD_RAM_OUTPUT_REGISTER : string  := "OFF";
        ADD_USEDW_MSB_BIT       : string  := "OFF";
        CLOCKS_ARE_SYNCHRONIZED : string  := "FALSE";
        DELAY_RDUSEDW           : natural := 1;
        DELAY_WRUSEDW           : natural := 1;
        INTENDED_DEVICE_FAMILY  : string  := "UNUSED";
        LPM_NUMWORDS            : natural := 4096;
        LPM_SHOWAHEAD           : string  := "ON";
        LPM_WIDTH               : natural := 32;
        LPM_WIDTH_R             : natural := 32;
        OVERFLOW_CHECKING       : string  := "ON";
        RDSYNC_DELAYPIPE        : natural := 5;
        READ_ACLR_SYNCH         : string  := "ON";
        UNDERFLOW_CHECKING      : string  := "ON";
        USE_EAB                 : string  := "ON";
        WRITE_ACLR_SYNCH        : string  := "ON";
        WRSYNC_DELAYPIPE        : natural := 5
    );
    port (
        aclr       : in  std_logic := '0';
        data       : in  std_logic_vector(LPM_WIDTH-1 downto 0);
        rdclk      : in  std_logic;
        rdreq      : in  std_logic;
        wrclk      : in  std_logic;
        wrreq      : in  std_logic;
        q          : out std_logic_vector(LPM_WIDTH_R-1 downto 0);
        rdempty    : out std_logic;
        rdfull     : out std_logic;
        rdusedw    : out std_logic_vector(compute_rdusedw_high(LPM_NUMWORDS,
                                                               LPM_WIDTH,
                                                               LPM_WIDTH_R,
                                                               ADD_USEDW_MSB_BIT) downto 0);
        wrempty    : out std_logic;
        wrfull     : out std_logic;
        wrusedw    : out std_logic_vector(compute_wrusedw_high(LPM_NUMWORDS,
                                                               ADD_USEDW_MSB_BIT) downto 0)
        --eccstatus  : out std_logic_vector(1 downto 0)
    );
end entity;

architecture arch of common_dcfifo is

    function valid_width_ratio( width : natural; width_r : natural ) return boolean is
        variable r  : natural := 0;
        variable rv : boolean := false;
    begin
        if( width > width_r ) then
            r := width / width_r;
        else
            r := width_r / width;
        end if;
        case r is
            when 1 | 2 | 4 | 8 | 16 | 32 => rv := true;
            when others                  => rv := false;
        end case;
        return rv;
    end function;

    constant LPM_HINT   : string := "UNUSED";
    constant ENABLE_ECC : string := "FALSE";

begin

    -- ------------------------------------------------------------------------
    -- Parameter checking. Should cause synthesis to error-out in Quartus.
    -- ------------------------------------------------------------------------

    assert valid_width_ratio( LPM_WIDTH, LPM_WIDTH_R )
        report   "Invalid width ratio in mixed width DCFIFO!"
        severity failure;

    assert (LPM_NUMWORDS >= 4)
        report   "Invalid FIFO depth (" & integer'image(LPM_NUMWORDS) & "). Must be >= 4."
        severity failure;

    -- ------------------------------------------------------------------------
    -- MIXED WIDTH DCFIFO
    --   We could just always use a mixed width DCFIFO. It's legal to have
    --   the same input/output port widths in this mode, but (unconfirmed)
    --   it may use more resources (maybe?).
    -- ------------------------------------------------------------------------

    mixed_fifo_gen : if( LPM_WIDTH /= LPM_WIDTH_R ) generate
        U_dcfifo_mixed_widths : dcfifo_mixed_widths
            generic map (
                ADD_RAM_OUTPUT_REGISTER => ADD_RAM_OUTPUT_REGISTER,
                ADD_USEDW_MSB_BIT       => ADD_USEDW_MSB_BIT,
                CLOCKS_ARE_SYNCHRONIZED => CLOCKS_ARE_SYNCHRONIZED,
                DELAY_RDUSEDW           => DELAY_RDUSEDW,
                DELAY_WRUSEDW           => DELAY_WRUSEDW,
                INTENDED_DEVICE_FAMILY  => INTENDED_DEVICE_FAMILY,
                --ENABLE_ECC              => ENABLE_ECC,
                LPM_NUMWORDS            => LPM_NUMWORDS,
                LPM_SHOWAHEAD           => LPM_SHOWAHEAD,
                LPM_WIDTH               => LPM_WIDTH,
                LPM_WIDTH_R             => LPM_WIDTH_R,
                LPM_WIDTHU              => wrusedw'length,
                LPM_WIDTHU_R            => rdusedw'length,
                OVERFLOW_CHECKING       => OVERFLOW_CHECKING,
                RDSYNC_DELAYPIPE        => RDSYNC_DELAYPIPE,
                READ_ACLR_SYNCH         => READ_ACLR_SYNCH,
                UNDERFLOW_CHECKING      => UNDERFLOW_CHECKING,
                USE_EAB                 => USE_EAB,
                WRITE_ACLR_SYNCH        => WRITE_ACLR_SYNCH,
                WRSYNC_DELAYPIPE        => WRSYNC_DELAYPIPE,
                LPM_HINT                => LPM_HINT,
                LPM_TYPE                => "dcfifo_mixed_widths"
            )
            port map (
                aclr      => aclr,
                data      => data,
                rdclk     => rdclk,
                rdreq     => rdreq,
                wrclk     => wrclk,
                wrreq     => wrreq,
                q         => q,
                rdempty   => rdempty,
                rdfull    => rdfull,
                rdusedw   => rdusedw,
                wrempty   => wrempty,
                wrfull    => wrfull,
                wrusedw   => wrusedw
             --eccstatus => eccstatus
            );
    end generate;


    -- ------------------------------------------------------------------------
    -- DCFIFO
    --   When read/write port widths are equal.
    -- ------------------------------------------------------------------------

    fifo_gen : if( LPM_WIDTH = LPM_WIDTH_R ) generate
        U_dcfifo : dcfifo
            generic map (
                ADD_RAM_OUTPUT_REGISTER => ADD_RAM_OUTPUT_REGISTER,
                ADD_USEDW_MSB_BIT       => ADD_USEDW_MSB_BIT,
                CLOCKS_ARE_SYNCHRONIZED => CLOCKS_ARE_SYNCHRONIZED,
                DELAY_RDUSEDW           => DELAY_RDUSEDW,
                DELAY_WRUSEDW           => DELAY_WRUSEDW,
                INTENDED_DEVICE_FAMILY  => INTENDED_DEVICE_FAMILY,
                --ENABLE_ECC              => ENABLE_ECC,
                LPM_NUMWORDS            => LPM_NUMWORDS,
                LPM_SHOWAHEAD           => LPM_SHOWAHEAD,
                LPM_WIDTH               => LPM_WIDTH,
                LPM_WIDTHU              => wrusedw'length,
                OVERFLOW_CHECKING       => OVERFLOW_CHECKING,
                RDSYNC_DELAYPIPE        => RDSYNC_DELAYPIPE,
                READ_ACLR_SYNCH         => READ_ACLR_SYNCH,
                UNDERFLOW_CHECKING      => UNDERFLOW_CHECKING,
                USE_EAB                 => USE_EAB,
                WRITE_ACLR_SYNCH        => WRITE_ACLR_SYNCH,
                WRSYNC_DELAYPIPE        => WRSYNC_DELAYPIPE,
                LPM_HINT                => LPM_HINT,
                LPM_TYPE                => "dcfifo"
            )
            port map (
                aclr      => aclr,
                data      => data,
                rdclk     => rdclk,
                rdreq     => rdreq,
                wrclk     => wrclk,
                wrreq     => wrreq,
                q         => q,
                rdempty   => rdempty,
                rdfull    => rdfull,
                rdusedw   => rdusedw,
                wrempty   => wrempty,
                wrfull    => wrfull,
                wrusedw   => wrusedw
             --eccstatus => eccstatus
            );
    end generate;

end architecture;
