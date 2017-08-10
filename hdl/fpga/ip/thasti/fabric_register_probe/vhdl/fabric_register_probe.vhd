-- Copyright (c) 2017 Stefan Biereigel
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

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity fabric_register_probe is
    port (
        mm_begintransfer : in  std_logic                     := '0';
        mm_write         : in  std_logic                     := '0';
        mm_address       : in  std_logic_vector(4 downto 0)  := (others => '0');
        mm_writedata     : in  std_logic_vector(31 downto 0) := (others => '0');
        mm_read          : in  std_logic                     := '0';
        mm_readdata      : out std_logic_vector(31 downto 0);
        mm_readdatavalid : out std_logic;
        clk              : in  std_logic                     := '0';
        reset            : in  std_logic                     := '0';
        register0_out    : out  std_logic_vector(31 downto 0);
        register1_out    : out  std_logic_vector(31 downto 0);
        register2_out    : out  std_logic_vector(31 downto 0);
        register3_out    : out  std_logic_vector(31 downto 0);
        register4_out    : out  std_logic_vector(31 downto 0);
        register5_out    : out  std_logic_vector(31 downto 0);
        register6_out    : out  std_logic_vector(31 downto 0);
        register7_out    : out  std_logic_vector(31 downto 0);
        register0_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register1_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register2_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register3_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register4_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register5_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register6_in     : in  std_logic_vector(31 downto 0) := (others => '0');
        register7_in     : in  std_logic_vector(31 downto 0) := (others => '0')
);
end entity fabric_register_probe;

architecture rtl of fabric_register_probe is
    -- flip flops to store written values
    signal register0_out_ff : std_logic_vector(31 downto 0);
    signal register1_out_ff : std_logic_vector(31 downto 0);
    signal register2_out_ff : std_logic_vector(31 downto 0);
    signal register3_out_ff : std_logic_vector(31 downto 0);
    signal register4_out_ff : std_logic_vector(31 downto 0);
    signal register5_out_ff : std_logic_vector(31 downto 0);
    signal register6_out_ff : std_logic_vector(31 downto 0);
    signal register7_out_ff : std_logic_vector(31 downto 0);
 
begin

    register0_out <= register0_out_ff;
    register1_out <= register1_out_ff;
    register2_out <= register2_out_ff;
    register3_out <= register3_out_ff;
    register4_out <= register4_out_ff;
    register5_out <= register5_out_ff;
    register6_out <= register6_out_ff;
    register7_out <= register7_out_ff;
    
    -- Avalon-MM interface for write requests
    mm_writes : process(clk)
    begin
        if rising_edge(clk) then
            if mm_write = '1' then
                case to_integer(unsigned(mm_address)) is
                    when  0| 1| 2| 3 => register0_out_ff <= mm_writedata;
                    when  4| 5| 6| 7 => register1_out_ff <= mm_writedata;
                    when  8| 9|10|11 => register2_out_ff <= mm_writedata;
                    when 12|13|14|15 => register3_out_ff <= mm_writedata;
                    when 16|17|18|19 => register4_out_ff <= mm_writedata;
                    when 20|21|22|23 => register5_out_ff <= mm_writedata;
                    when 24|25|26|27 => register6_out_ff <= mm_writedata;
                    when 28|29|30|31 => register7_out_ff <= mm_writedata;
                    when others => null;
                end case;
            end if;
        end if;
    end process;

    -- Avanlon-MM interface for read requests
    mm_reads : process(clk)
    begin
        if rising_edge(clk) then
            mm_readdatavalid <= mm_read;
            if mm_read = '1' then
                case to_integer(unsigned(mm_address)) is
                    when  0| 1| 2| 3 => mm_readdata <= register0_in;
                    when  4| 5| 6| 7 => mm_readdata <= register1_in;
                    when  8| 9|10|11 => mm_readdata <= register2_in;
                    when 12|13|14|15 => mm_readdata <= register3_in;
                    when 16|17|18|19 => mm_readdata <= register4_in;
                    when 20|21|22|23 => mm_readdata <= register5_in;
                    when 24|25|26|27 => mm_readdata <= register6_in;
                    when 28|29|30|31 => mm_readdata <= register7_in;
                    when others => mm_readdata <= (others => '0');
                end case;
            end if;
        end if;
    end process;

end architecture rtl;
