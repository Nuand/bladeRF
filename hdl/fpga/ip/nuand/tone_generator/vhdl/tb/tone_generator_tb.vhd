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
        CLOCK_FREQUENCY             : natural   := 1_920_000;
        TONE_FREQUENCY              : natural   := 1_000;
        WORDS_PER_MINUTE            : real      := 10.0
    );
end entity;

architecture tb_tgen of tone_generator_tb is
    signal clock    : std_logic     := '1';
    signal reset    : std_logic     := '1';

    signal inputs   : tone_generator_input_t    := (dphase      => 0,
                                                    duration    => 0,
                                                    valid       => '0');
    signal outputs  : tone_generator_output_t;

    constant CLOCK_PERIOD   : time := 1 sec / CLOCK_FREQUENCY;
    constant DOT            : time := (60.0 sec/50.0) / WORDS_PER_MINUTE;
    constant DASH           : time := DOT * 3;
    constant PAUSE          : time := DOT * 3;
    constant SPACE          : time := DOT * 7;

    constant TONE_PERIOD        : time      := 1 sec / TONE_FREQUENCY;
    constant TONE_PERIOD_CLKS   : natural   := TONE_PERIOD / CLOCK_PERIOD;
    constant TONE_DPHASE        : natural   := integer(round(8192.0 / real(TONE_PERIOD_CLKS)));

    type tone_t is record
        is_on   : boolean;
        duration: time;
    end record;

    type tones_t is array(natural range <>) of tone_t;

    constant MESSAGE : tones_t := (
        (is_on => false, duration => PAUSE),

        -- H
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- E
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- L
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- L
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- O
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => PAUSE)
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
                inputs.dphase <= TONE_DPHASE;
            else
                inputs.dphase <= 0;
            end if;

            inputs.duration <= MESSAGE(i).duration / CLOCK_PERIOD;

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
    signal clock    : std_logic     := '1';
    signal reset    : std_logic     := '1';

    signal inputs   : tone_generator_input_t    := (dphase      => 0,
                                                    duration    => 0,
                                                    valid       => '0');
    signal outputs  : tone_generator_output_t;

    signal mm_addr        : std_logic_vector(3 downto 0);
    signal mm_din         : std_logic_vector(31 downto 0);
    signal mm_dout        : std_logic_vector(31 downto 0);
    signal mm_write       : std_logic;
    signal mm_read        : std_logic;
    signal mm_waitreq     : std_logic;
    signal mm_readack     : std_logic;
    signal mm_intr        : std_logic;

    constant CLOCK_PERIOD   : time := 1 sec / CLOCK_FREQUENCY;
    constant DOT            : time := (60.0 sec/50.0) / WORDS_PER_MINUTE;
    constant DASH           : time := DOT * 3;
    constant PAUSE          : time := DOT * 3;
    constant SPACE          : time := DOT * 7;

    constant TONE_PERIOD        : time      := 1 sec / TONE_FREQUENCY;
    constant TONE_PERIOD_CLKS   : natural   := TONE_PERIOD / CLOCK_PERIOD;
    constant TONE_DPHASE        : natural   := integer(round(8192.0 / real(TONE_PERIOD_CLKS)));

    type tone_t is record
        is_on   : boolean;
        duration: time;
    end record;

    type tones_t is array(natural range <>) of tone_t;

    constant MESSAGE : tones_t := (
        (is_on => false, duration => PAUSE),

        -- H
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- E
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- L
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- L
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DOT),
        (is_on => false, duration => PAUSE),

        -- O
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => DOT),
        (is_on => true, duration => DASH),
        (is_on => false, duration => PAUSE)
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

    U_tone_generator_hw : entity work.tone_generator_hw
        port map (
            clock       => clock,
            reset       => reset,
            tgen_out    => inputs,
            tgen_in     => outputs,
            addr        => mm_addr,
            din         => mm_din,
            dout        => mm_dout,
            write       => mm_write,
            read        => mm_read,
            waitreq     => mm_waitreq,
            readack     => mm_readack,
            intr        => mm_intr
        );

    tb : process
        variable dphase : natural;
        variable duration : natural;
        variable control : std_logic_vector(mm_din'range);
        variable status : std_logic_vector(mm_dout'range);
        variable done : boolean;
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
            done := false;

            wait_for_queue_space : while not done loop
                wait until rising_edge(clock);
                mm_addr <= std_logic_vector(to_unsigned(0, mm_addr'length));
                mm_read <= '1';
                wait until rising_edge(clock);
                wait until (mm_readack = '1');
                status  := mm_dout;
                mm_read <= '0';
                wait until rising_edge(clock);

                done := (status(3) = '0');
            end loop wait_for_queue_space;

            wait until rising_edge(clock);
            mm_addr <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_din  <= control;
            mm_read <= '1';
            wait until rising_edge(clock);
            wait until (mm_readack = '1');
            status  := mm_dout;
            mm_read <= '0';
            wait until rising_edge(clock);

            dphase   := TONE_DPHASE when MESSAGE(i).is_on else 0;
            duration := MESSAGE(i).duration / CLOCK_PERIOD;
            control  := (0 => '1', 2 => '1', others => '0');

            if (mm_waitreq = '1') then
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(1, mm_addr'length));
            mm_din   <= std_logic_vector(to_unsigned(dphase, mm_din'length));
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            if (mm_waitreq = '1') then
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(2, mm_addr'length));
            mm_din   <= std_logic_vector(to_unsigned(duration, mm_din'length));
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            if (mm_waitreq = '1') then
                wait until (mm_waitreq = '0');
            end if;

            mm_addr  <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_din   <= control;
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);
        end loop send_message;

        done := false;

        wait_for_done : while not done loop
            report "waiting for irq";

            wait until (mm_intr = '1');
            wait until rising_edge(clock);
            mm_addr  <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_din   <= (0 => '1', others => '0');
            mm_write <= '1';
            wait until rising_edge(clock);
            mm_write <= '0';
            wait until rising_edge(clock);

            mm_addr <= std_logic_vector(to_unsigned(0, mm_addr'length));
            mm_read <= '1';
            wait until rising_edge(clock);
            wait until (mm_readack = '1');
            status  := mm_dout;
            mm_read <= '0';
            wait until rising_edge(clock);

            done := (status(2) = '1');
        end loop wait_for_done;

        -- done
        nop(clock, 100);
        report "-- End of Simulation --" severity failure ;
    end process tb;

end architecture tb_hw;
