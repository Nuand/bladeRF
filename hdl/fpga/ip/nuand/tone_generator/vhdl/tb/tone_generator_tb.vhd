-- Copyright (c) 2019 Nuand LLC
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

library std;
    use std.env.all;
    use std.textio.all;

library work;
    use work.util.all;
    use work.tone_generator_p.all;

entity tone_generator_tb is
    generic (
        SAMPLE_CLOCK_FREQ           : natural   := 1_920_000;
        SYSTEM_CLOCK_FREQ           : natural   := 4_000_000;
        TONE_FREQUENCY              : natural   := 1_000;
        WORDS_PER_MINUTE            : real      := 10.0
    );
end entity;

architecture tb_tgen of tone_generator_tb is
    signal clock    : std_logic     := '1';
    signal reset    : std_logic     := '1';

    signal inputs   : tone_generator_input_t := (dphase   => (others => '0'),
                                                 duration => (others => '0'),
                                                 valid    => '0');
    signal outputs  : tone_generator_output_t;

    constant CLOCK_PERIOD   : time      := 1 sec / SAMPLE_CLOCK_FREQ;
    constant DOT            : time      := (60.0 sec/50.0) / WORDS_PER_MINUTE;
    constant DASH           : time      := DOT * 3;
    constant PAUSE          : time      := DOT * 3;
    constant SPACE          : time      := DOT * 7;

    constant TONE_PERIOD    : time      := 1 sec / TONE_FREQUENCY;
    constant TONE_PER_CLKS  : natural   := TONE_PERIOD / CLOCK_PERIOD;
    constant TONE_DPHASE    : natural   := integer(round(8192.0 / real(TONE_PER_CLKS)));

    type tone_t is record
        is_on   : boolean;
        duration: time;
    end record;

    type tones_t is array(natural range <>) of tone_t;

    constant MESSAGE : tones_t := (
        (is_on => false,    duration => PAUSE),

        -- H
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- E
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- L
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- L
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- O
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => PAUSE)
    );
begin

    clock <= not clock after CLOCK_PERIOD/2;

    U_tone_generator : entity work.tone_generator
        port map (
            clock   => clock,
            reset   => reset,
            inputs  => inputs,
            outputs => outputs
        );

    tb : process
    begin
        -- reset
        reset <= '1';
        nop(clock, 10);

        reset <= '0';
        nop(clock, 10);

        -- send message
        for i in MESSAGE'range loop
            if MESSAGE(i).is_on then
                inputs.dphase <= to_signed(TONE_DPHASE, inputs.dphase'length);
            else
                inputs.dphase <= (others => '0');
            end if;

            inputs.duration <= to_unsigned((MESSAGE(i).duration / CLOCK_PERIOD),
                                           inputs.duration'length);

            inputs.valid <= '1';
            wait until rising_edge(clock);
            inputs.valid <= '0';
            wait until rising_edge(clock);
            wait until (outputs.active = '1');
            wait until (outputs.active = '0');
        end loop;

        -- done
        nop(clock, 100);
        report "-- End of Simulation --" severity failure ;
    end process tb;

end architecture tb_tgen;

architecture tb_hw of tone_generator_tb is
    signal clock            : std_logic := '1';
    signal reset            : std_logic := '1';

    signal mm_addr          : std_logic_vector(1 downto 0);
    signal mm_din           : std_logic_vector(31 downto 0);
    signal mm_dout          : std_logic_vector(31 downto 0);
    signal mm_write         : std_logic;
    signal mm_read          : std_logic;
    signal mm_waitreq       : std_logic;
    signal mm_readack       : std_logic;
    signal mm_queue_empty   : std_logic;
    signal mm_queue_full    : std_logic;
    signal mm_running       : std_logic;

    signal sample_clk       : std_logic := '1';
    signal sample_i         : signed(15 downto 0);
    signal sample_q         : signed(15 downto 0);
    signal sample_valid     : std_logic;

    constant CLOCK_PERIOD   : time      := 1 sec / SAMPLE_CLOCK_FREQ;
    constant DOT            : time      := (60.0 sec/50.0) / WORDS_PER_MINUTE;
    constant DASH           : time      := DOT * 3;
    constant PAUSE          : time      := DOT * 3;
    constant SPACE          : time      := DOT * 7;

    constant TONE_PERIOD    : time      := 1 sec / TONE_FREQUENCY;
    constant TONE_PER_CLKS  : natural   := TONE_PERIOD / CLOCK_PERIOD;
    constant TONE_DPHASE    : natural   := integer(round(8192.0 / real(TONE_PER_CLKS)));

    type tone_t is record
        is_on   : boolean;
        duration: time;
    end record;

    type tones_t is array(natural range <>) of tone_t;

    constant MESSAGE : tones_t := (
        (is_on => false,    duration => 1024*CLOCK_PERIOD),
        (is_on => true,     duration => 1024*CLOCK_PERIOD),
        (is_on => false,    duration => 1024*CLOCK_PERIOD),
        (is_on => true,     duration => 1024*CLOCK_PERIOD),
        (is_on => true,     duration => 1024*CLOCK_PERIOD),
        (is_on => false,    duration => 1024*CLOCK_PERIOD),
        (is_on => false,    duration => 1024*CLOCK_PERIOD),

        -- H
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- E
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- L
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- L
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DOT),
        (is_on => false,    duration => PAUSE),

        -- O
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => DOT),
        (is_on => true,     duration => DASH),
        (is_on => false,    duration => PAUSE),

        (is_on => false,    duration => 1024*CLOCK_PERIOD)
    );
begin

    clock       <= not clock after (1 sec/SYSTEM_CLOCK_FREQ)/2;
    sample_clk  <= not sample_clk after CLOCK_PERIOD/2;

    U_tone_generator_hw : entity work.tone_generator_hw
        port map (
            clock           => clock,
            reset           => reset,

            sample_clk      => sample_clk,
            sample_i        => sample_i,
            sample_q        => sample_q,
            sample_valid    => sample_valid,

            addr            => mm_addr,
            din             => mm_din,
            dout            => mm_dout,
            write           => mm_write,
            read            => mm_read,
            waitreq         => mm_waitreq,
            readack         => mm_readack,

            queue_empty     => mm_queue_empty,
            queue_full      => mm_queue_full,
            running         => mm_running
        );

    tb : process
        variable dphase     : natural;
        variable duration   : natural;
    begin
        -- reset
        reset <= '1';
        nop(clock, 10);

        reset <= '0';
        nop(clock, 10);

        -- send message
        send_message : for i in MESSAGE'range loop
            report "loading message " & to_string(i);

            -- check to make sure there's room...
            if (mm_queue_full = '1') then
                report "mm_queue_full blocking message load";
                wait until (mm_queue_full = '0');
            end if;

            wait until rising_edge(clock);

            -- write dphase
            dphase   := TONE_DPHASE when MESSAGE(i).is_on else 0;

            if (mm_waitreq = '1') then
                report "mm_waitreq blocking addr 1 write";
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(1, mm_addr'length));
            mm_din   <= std_logic_vector(to_unsigned(dphase, mm_din'length));
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            -- write duration
            duration := MESSAGE(i).duration / CLOCK_PERIOD;

            if (mm_waitreq = '1') then
                report "mm_waitreq blocking addr 2 write";
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(2, mm_addr'length));
            mm_din   <= std_logic_vector(to_unsigned(duration, mm_din'length));
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            -- send append command
            if (mm_waitreq = '1') then
                report "mm_waitreq blocking addr 0 write";
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_din   <= (0 => '1', others => '0');
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            -- read status reg for length
            mm_addr  <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_read  <= '1';
            wait until rising_edge(clock) and (mm_readack = '1');
            mm_read  <= '0';
            report "status:" &
                   " msg=" & to_string(i) &
                   " run=" & to_string(mm_dout(0)) &
                   " full=" & to_string(mm_dout(1)) &
                   " len=" & to_string(to_integer(unsigned(
                                                    mm_dout(15 downto 8))));
            wait until rising_edge(clock);
        end loop send_message;

        if (mm_queue_full = '1') then
            report "waiting for not queue_full";
            wait until (mm_queue_full = '0');
        end if;

        if (mm_queue_empty = '0') then
            report "waiting for queue_empty";
            wait until (mm_queue_empty = '1');
        end if;

        if (mm_running = '1') then
            report "waiting for not running";
            wait until (mm_running = '0');
        end if;

        -- done
        nop(clock, 100);
        report "-- End of Simulation --" severity failure;
    end process tb;

end architecture tb_hw;
