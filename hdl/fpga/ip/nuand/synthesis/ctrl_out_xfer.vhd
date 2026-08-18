-- Copyright (c) 2026 Distributed Spectrum
--
-- Atomic transfer of the AD9361 CTRL_OUT byte between clock domains.
--
-- The AD9361 drives CTRL_OUT from its own internal clock and provides no strobe
-- alongside it -- the "Gain Change" pulse lives on a different CTRL_OUT row
-- (UG-570 Table 44) and only one row can be selected at a time. So there is no
-- source-synchronous interface here and no write enable, which rules out
-- capturing the pins straight into a FIFO: the first hop into any clock domain
-- is unavoidably per-bit.
--
-- What can be made atomic is everything after that hop, which matters because a
-- torn byte is not a harmless glitch -- it decodes as a wildly wrong gain index.
-- This block takes a byte already synchronized to src_clock, waits only until it
-- is settled, and then hands all eight bits across in a single parallel load.

library ieee ;
    use ieee.std_logic_1164.all ;

entity ctrl_out_xfer is
  port (
    -- Source domain. src_data must already be synchronized to src_clock; on the
    -- bladeRF micro the existing per-bit synchronizers that feed the Nios
    -- readback register are reused for this.
    src_clock       :   in  std_logic ;
    src_reset       :   in  std_logic ;
    src_data        :   in  std_logic_vector(7 downto 0) ;

    -- Destination domain
    dst_clock       :   in  std_logic ;
    dst_reset       :   in  std_logic ;
    dst_data        :   out std_logic_vector(7 downto 0) := (others => '0') ;
    dst_valid       :   out std_logic := '0'
  ) ;
end entity ;

architecture arch of ctrl_out_xfer is

    -- Data plus a validity bit, carried through the handshake together so the
    -- two can never disagree on the destination side.
    constant XFER_WIDTH     :   positive := 9 ;

    signal src_prev         :   std_logic_vector(src_data'range) := (others => '0') ;
    signal src_stable       :   std_logic_vector(src_data'range) := (others => '0') ;
    signal src_stable_valid :   std_logic := '0' ;

    signal xfer_data        :   std_logic_vector(XFER_WIDTH-1 downto 0) ;
    signal req              :   std_logic := '0' ;
    signal ack              :   std_logic ;

begin

    -- Settling guard.
    --
    -- Each CTRL_OUT bit reaches src_clock through its own equal-depth three-flop
    -- synchronizer, so during a transition the bits can resolve on different
    -- cycles and the byte is briefly a mixture of old and new. Accept a value
    -- only once two consecutive samples agree. That is the minimum check that
    -- can establish settledness -- not a tuned delay -- and it is sufficient
    -- here: equal-depth chains bound the spread to a single cycle, so a given
    -- mixture cannot survive two samples in a row, while the AD9361 holds the
    -- gain index for at least one gain update interval (1 ms by default, tens of
    -- thousands of src_clock cycles).
    settle : process(src_clock, src_reset)
    begin
        if( src_reset = '1' ) then
            src_prev         <= (others => '0') ;
            src_stable       <= (others => '0') ;
            src_stable_valid <= '0' ;
        elsif( rising_edge(src_clock) ) then
            if( src_data = src_prev ) then
                src_stable       <= src_data ;
                src_stable_valid <= '1' ;
            end if ;
            src_prev <= src_data ;
        end if ;
    end process ;

    -- All nine bits are latched into the handshake's holding register on one
    -- edge, so the destination can never observe a partial update.
    U_handshake : entity work.handshake
      generic map (
        DATA_WIDTH      =>  XFER_WIDTH
      ) port map (
        source_clock    =>  src_clock,
        source_reset    =>  src_reset,
        source_data     =>  src_stable_valid & src_stable,

        dest_clock      =>  dst_clock,
        dest_reset      =>  dst_reset,
        dest_data       =>  xfer_data,
        dest_req        =>  req,
        dest_ack        =>  ack
      ) ;

    -- Free-running requester, same idiom as U_handshake_timestamp in
    -- bladerf-hosted.vhd. dest_data is a combinational tap of the source-domain
    -- holding register, which is stable whenever ack is asserted, so latch it
    -- there to give the rest of the destination domain a registered byte.
    --
    -- One round trip is on the order of ten cycles, which refreshes far faster
    -- than the AGC can change the gain.
    refresh : process(dst_clock, dst_reset)
    begin
        if( dst_reset = '1' ) then
            req       <= '0' ;
            dst_data  <= (others => '0') ;
            dst_valid <= '0' ;
        elsif( rising_edge(dst_clock) ) then
            if( ack = '0' ) then
                req <= '1' ;
            else
                req       <= '0' ;
                dst_data  <= xfer_data(7 downto 0) ;
                dst_valid <= xfer_data(8) ;
            end if ;
        end if ;
    end process ;

end architecture ;
