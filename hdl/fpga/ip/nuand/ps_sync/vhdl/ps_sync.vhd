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
    use ieee.math_real.all;

-- ----------------------------------------------------------------------------
-- Power supply synchronization controller
-- ----------------------------------------------------------------------------
entity ps_sync is
    generic (
        OUTPUTS   : natural        := 1;    -- Number of ps sync signals
        USE_LFSR  : boolean        := true; -- Use LFSR to cycle through freqs
        HOP_LIST  : integer_vector;         -- Array of number of refclk cycles
                                            -- that sync remains at '0' or '1'.
        HOP_RATE  : natural        := 1     -- Number of times to repeat each
                                            -- hop freq. before moving on.
    );
    port (
        refclk    : in    std_logic;
        sync      : out   std_logic_vector(OUTPUTS-1 downto 0) := (others => '0')
    );
end entity;

architecture arch of ps_sync is

begin

    sync_proc : process(refclk)
        variable count      : natural;
        variable count_rate : natural;
        variable sync_v     : std_logic                    := '0';
        variable sel        : natural range HOP_LIST'range := HOP_LIST'low;

        -- A 10-bit maximal length LFSR should be plenty for the number of hop freqs
        constant LFSR_SEED  : unsigned(9 downto 0)         := 10x"281";
        variable lfsr       : unsigned(LFSR_SEED'range)    := LFSR_SEED;

        -- FSM
        type fsm_t is (
            LOAD_COUNT,
            DOWNCOUNT_LOW,
            DOWNCOUNT_HIGH
        );
        variable state      : fsm_t := LOAD_COUNT;

    begin
        if( rising_edge(refclk) ) then

            case state is

                when LOAD_COUNT =>

                    if( USE_LFSR = true ) then
                        sel := to_integer(lfsr) mod HOP_LIST'length;
                    end if;

                    count      := HOP_LIST(sel) - 1; -- this cycle counts as the first
                    count_rate := HOP_RATE;
                    sync_v     := '0';
                    state      := DOWNCOUNT_LOW;

                when DOWNCOUNT_LOW =>

                    sync_v := '0';

                    if( count = 1 ) then
                        sync_v := '1';
                        count  := HOP_LIST(sel);
                        state  := DOWNCOUNT_HIGH;
                    else
                        count  := count - 1;
                    end if;

                when DOWNCOUNT_HIGH =>

                    sync_v := '1';

                    if( count = 1 ) then
                        if( count_rate = 1 ) then
                            if( USE_LFSR = true ) then
                                lfsr := lfsr(lfsr'high-1 downto lfsr'low) & (lfsr(9) xor lfsr(6));
                            else
                                sel  := (sel + 1) mod HOP_LIST'length;
                            end if;
                            sync_v := '0';
                            state  := LOAD_COUNT;
                        else
                            sync_v     := '0';
                            count      := HOP_LIST(sel);
                            count_rate := count_rate - 1;
                            state      := DOWNCOUNT_LOW;
                        end if;
                    else
                        count  := count - 1;
                    end if;

                when others =>

                    sel    := HOP_LIST'low;
                    sync_v := '0';
                    state  := LOAD_COUNT;

            end case;

            sync <= (others => sync_v);

        end if;
    end process;

end architecture;
