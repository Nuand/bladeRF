library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity fifo_writer is
  port (
    clock               :   in      std_logic ;
    reset               :   in      std_logic ;
    enable              :   in      std_logic ;

    usb_speed           :   in      std_logic ;
    meta_en             :   in      std_logic ;
    timestamp           :   in      unsigned(63 downto 0);

    in_i                :   in      signed(15 downto 0) ;
    in_q                :   in      signed(15 downto 0) ;
    in_valid            :   in      std_logic ;

    fifo_usedw          :   in      std_logic_vector(11 downto 0) ;
    fifo_clear          :   buffer  std_logic ;
    fifo_write          :   buffer  std_logic ;
    fifo_full           :   in      std_logic ;
    fifo_data           :   out     std_logic_vector(31 downto 0) ;

    meta_fifo_full      :   in     std_logic;
    meta_fifo_usedw     :   in     std_logic_vector(4 downto 0);
    meta_fifo_data      :   out    std_logic_vector(127 downto 0);
    meta_fifo_write     :   out    std_logic;

    overflow_led        :   buffer  std_logic ;
    overflow_count      :   buffer  unsigned(63 downto 0) ;
    overflow_duration   :   in      unsigned(15 downto 0)
  ) ;
end entity ;

architecture simple of fifo_writer is

    signal buf_enough    : std_logic;
    signal dma_buf_sz    : signed(12 downto 0);
    signal dma_downcount : signed(12 downto 0);
    signal overflow_recovering  :   std_logic ;
    signal overflow_detected    :   std_logic ;

begin

    dma_buf_sz <= to_signed(1015, dma_buf_sz'length) when usb_speed = '0' else to_signed(503, dma_downcount'length);
    process( clock, reset)
    begin
        if( reset = '1') then
            dma_downcount <= dma_buf_sz;
        elsif( rising_edge( clock ) ) then
            if (enable = '1' and meta_en = '1') then
                if ( dma_downcount <= 0) then
                    dma_downcount <= dma_buf_sz;
                else
                    dma_downcount <= dma_downcount - 1;
                end if;
            else
                dma_downcount <= dma_buf_sz;
            end if;
        end if;
    end process;

    buf_enough <= '1' when ( dma_downcount = (dma_buf_sz)) else '0';

    meta_fifo_write <= '1' when (enable = '1' and meta_en = '1' and buf_enough = '1') else '0';
    meta_fifo_data <= x"FFFFFFFF" & std_logic_vector(timestamp) & x"12344321";

    -- Simple concatenation of samples
    fifo_data   <= std_logic_vector(in_q & in_i) ;
    fifo_write  <= in_valid when overflow_recovering = '0' and fifo_full = '0' else '0' ;

    -- Clear out the contents when RX is disabled
    clear_fifo : process( clock, reset )
    begin
        if( reset = '1' ) then
            fifo_clear <= '1' ;
        elsif( rising_edge(clock) ) then
            fifo_clear <= '0' ;
            if( enable = '0' ) then
                fifo_clear <= '1' ;
            end if ;
        end if ;
    end process ;

    -- Overflow detection
    detect_overflows : process( clock, reset )
    begin
        if( reset = '1' ) then
            overflow_detected <= '0' ;
        elsif( rising_edge( clock ) ) then
            overflow_detected <= '0' ;
            if( enable = '1' and in_valid = '1' and fifo_full = '1' and fifo_clear = '0' ) then
                overflow_detected <= '1' ;
            end if ;
        end if ;
    end process ;

    -- Count the number of times we overflow, but only if they are discontinuous
    -- meaning we have an overflow condition, a non-overflow condition, then
    -- another overflow condition counts as 2 overflows, but an overflow condition
    -- followed by N overflow conditions counts as a single overflow condition.
    count_overflows : process( clock, reset )
        variable prev_overflow : std_logic := '0' ;
    begin
        if( reset = '1' ) then
            prev_overflow := '0' ;
            overflow_count <= (others =>'0') ;
            overflow_recovering <= '0' ;
        elsif( rising_edge( clock ) ) then
            overflow_recovering <= '0' ;
            if( prev_overflow = '0' and overflow_detected = '1' ) then
                overflow_count <= overflow_count + 1 ;
                overflow_recovering <= '1' ;
            elsif( overflow_recovering = '1' ) then
                if( fifo_full = '0' ) then
                    overflow_recovering <= '0' ;
                end if ;
            end if ;
            prev_overflow := overflow_detected ;
        end if ;
    end process ;

    -- Active high assertion for overflow_duration when the overflow
    -- condition has been detected.  The LED will stay asserted
    -- if multiple overflows have occurred
    blink_overflow_led : process( clock, reset )
        variable downcount : natural range 0 to 2**overflow_duration'length-1 ;
    begin
        if( reset = '1' ) then
            downcount := 0 ;
            overflow_led <= '0' ;
        elsif( rising_edge(clock) ) then
            -- Default to not being asserted
            overflow_led <= '0' ;

            -- Countdown so we can see what happened
            if( overflow_detected = '1' ) then
                downcount := to_integer(overflow_duration) ;
            elsif( downcount > 0 ) then
                downcount := downcount - 1 ;
                overflow_led <= '1' ;
            end if ;

        end if ;
    end process ;

end architecture ;

