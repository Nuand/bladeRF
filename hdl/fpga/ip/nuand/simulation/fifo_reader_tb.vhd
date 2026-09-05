-- Copyright (c) 2026 Nuand LLC
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

-- Does the TX feed gate hold a burst until its timestamp arrives?
--
-- Two cases, both required:
--   scheduled  a burst timestamped in the future must produce NO sample
--              reads before that timestamp, and all of them after
--   now        a burst with timestamp 0 (the host's TX_NOW encoding, which
--              the reader turns into all ones by subtracting 1) must start
--              immediately
--
-- The scheduled case regressed when meta_p_time was derived from the
-- registered copy of meta_fifo_data: that copy is all zeros out of reset,
-- and 0 - 1 wraps to all ones, i.e. the META_NOW sentinel, so the gate
-- opened at once for a future burst.
--
-- Run with GHDL:
--   ghdl -a --std=08 fx3_gpif_p.vhd fifo_readwrite_p.vhd \
--                    fifo_reader.vhd fifo_reader_tb.vhd
--   ghdl -e --std=08 fifo_reader_tb
--   ghdl -r --std=08 fifo_reader_tb --stop-time=70us

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
library work;
    use work.fifo_readwrite_p.all;
    use work.fx3_gpif_p.all;

entity fifo_reader_tb is
    generic (
        -- Sample timestamp for the burst. 0 means "transmit now".
        TGT : natural := 4000
    );
end entity;

architecture sim of fifo_reader_tb is

    constant NSTREAMS : natural := 1;

    signal clock      : std_logic := '0';
    signal reset      : std_logic := '1';
    signal enable     : std_logic := '0';
    signal usb_speed  : std_logic := '0';   -- '0' = SuperSpeed
    signal meta_en    : std_logic := '1';
    signal packet_en  : std_logic := '0';
    signal eight_bit  : std_logic := '0';
    signal packed_en  : std_logic := '0';
    signal timestamp  : unsigned(63 downto 0) := (others => '0');

    signal fifo_usedw : std_logic_vector(11 downto 0) := (others => '1');
    signal fifo_read  : std_logic;
    signal fifo_empty : std_logic := '0';
    signal fifo_data  : std_logic_vector(63 downto 0) := x"0BAD0BAD0BAD0BAD";
    signal fifo_hold  : std_logic := '0';

    signal pkt_ctrl   : packet_control_t;
    signal pkt_empty  : std_logic;
    signal pkt_ready  : std_logic := '0';

    signal m_usedw    : std_logic_vector(2 downto 0) := "001";
    signal m_read     : std_logic;
    signal m_empty    : std_logic := '0';
    signal m_data     : std_logic_vector(127 downto 0);

    signal in_ctrl    : sample_controls_t(0 to NSTREAMS-1) := (others => SAMPLE_CONTROL_ENABLE);
    signal out_smp    : sample_streams_t(0 to NSTREAMS-1);

    signal uf_led     : std_logic;
    signal uf_count   : unsigned(63 downto 0);
    signal uf_dur     : unsigned(15 downto 0) := x"ffff";

    signal reads_before : natural := 0;
    signal reads_after  : natural := 0;
    signal done         : boolean := false;

    -- Bits 95 downto 32 of the meta header carry the timestamp.
    function hdr(ts : unsigned(63 downto 0)) return std_logic_vector is
        variable v : std_logic_vector(127 downto 0) := (others => '0');
    begin
        v(95 downto 32) := std_logic_vector(ts);
        return v;
    end function;

begin

    clock <= not clock after 5 ns when not done else '0';

    m_data <= hdr(to_unsigned(TGT, 64));

    -- One header only: once it has been read the meta FIFO is empty.
    -- Without this META_LOAD re-arms forever and the later reads would be
    -- legitimate, hiding the defect.
    meta_drain : process(clock)
    begin
        if rising_edge(clock) then
            if reset = '1' then
                m_empty <= '0';
            elsif m_read = '1' then
                m_empty <= '1';
            end if;
        end if;
    end process;

    -- The sample clock runs regardless, as it does in hardware.
    tick : process(clock)
    begin
        if rising_edge(clock) and reset = '0' then
            timestamp <= timestamp + 1;
        end if;
    end process;

    counters : process(clock)
    begin
        if rising_edge(clock) and reset = '0' and fifo_read = '1' then
            if timestamp < to_unsigned(TGT, 64) then
                reads_before <= reads_before + 1;
            else
                reads_after <= reads_after + 1;
            end if;
        end if;
    end process;

    U_fifo_reader : entity work.fifo_reader
        generic map (
            NUM_STREAMS           => NSTREAMS,
            FIFO_READ_THROTTLE    => 0,
            FIFO_USEDW_WIDTH      => 12,
            FIFO_DATA_WIDTH       => 64,
            META_FIFO_USEDW_WIDTH => 3,
            META_FIFO_DATA_WIDTH  => 128
        )
        port map (
            clock                 => clock,
            reset                 => reset,
            enable                => enable,
            usb_speed             => usb_speed,
            meta_en               => meta_en,
            packet_en             => packet_en,
            eight_bit_mode_en     => eight_bit,
            highly_packed_mode_en => packed_en,
            timestamp             => timestamp,
            fifo_usedw            => fifo_usedw,
            fifo_read             => fifo_read,
            fifo_empty            => fifo_empty,
            fifo_data             => fifo_data,
            fifo_holdoff          => fifo_hold,
            packet_control        => pkt_ctrl,
            packet_empty          => pkt_empty,
            packet_ready          => pkt_ready,
            meta_fifo_usedw       => m_usedw,
            meta_fifo_read        => m_read,
            meta_fifo_empty       => m_empty,
            meta_fifo_data        => m_data,
            in_sample_controls    => in_ctrl,
            out_samples           => out_smp,
            underflow_led         => uf_led,
            underflow_count       => uf_count,
            underflow_duration    => uf_dur
        );

    stim : process
    begin
        wait for 50 ns;
        reset  <= '0';
        wait for 20 ns;
        enable <= '1';

        wait for 60 us;

        report "TGT = " & integer'image(TGT) &
               "  reads before = " & integer'image(reads_before) &
               "  reads after = " & integer'image(reads_after);

        if TGT = 0 then
            assert reads_after > 0
                report "TX_NOW burst never started"
                severity failure;
            report "TX_NOW: feed started";
        else
            assert reads_before = 0
                report "gate leaked: feed ran before its timestamp"
                severity failure;
            assert reads_after > 0
                report "gate never opened at its timestamp"
                severity failure;
            report "scheduled: gate held until the timestamp";
        end if;

        done <= true;
        wait;
    end process;

end architecture;
