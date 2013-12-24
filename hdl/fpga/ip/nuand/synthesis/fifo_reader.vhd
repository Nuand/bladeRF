library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity fifo_reader is
  port (
    clock               :   in      std_logic ;
    reset               :   in      std_logic ;
    enable              :   in      std_logic ;

    meta_en             :   in      std_logic ;
    timestamp           :   in      unsigned(63 downto 0);

    fifo_usedw          :   in      std_logic_vector(11 downto 0);
    fifo_read           :   buffer  std_logic ;
    fifo_empty          :   in      std_logic ;
    fifo_data           :   in      std_logic_vector(31 downto 0) ;

    meta_fifo_usedw     :   in      std_logic_vector(2 downto 0);
    meta_fifo_read      :   buffer  std_logic ;
    meta_fifo_empty     :   in      std_logic ;
    meta_fifo_data      :   in      std_logic_vector(127 downto 0) ;

    out_i               :   buffer  signed(15 downto 0) ;
    out_q               :   buffer  signed(15 downto 0) ;
    out_valid           :   buffer  std_logic ;

    underflow_led       :   buffer  std_logic ;
    underflow_count     :   buffer  unsigned(63 downto 0) ;
    underflow_duration  :   in      unsigned(15 downto 0)
  ) ;
end entity ;

architecture simple of fifo_reader is

    signal underflow_detected   :   std_logic ;
    signal meta_time_go         :   std_logic ;
    signal meta_time_eq         :   std_logic ;
    signal meta_time_hit        :   signed(15 downto 0) ;
    signal meta_p_time          :   unsigned(63 downto 0);
    signal meta_p_sec           :   unsigned(31 downto 0);
    signal meta_loaded          :   std_logic;
    signal meta_loading         :   std_logic;
    signal meta_load_idx        :   unsigned(1 downto 0);


begin

    process(clock, reset, meta_fifo_empty, meta_fifo_usedw)
    begin
        if (reset = '1') then
            meta_loaded   <= '0';
            meta_loading  <= '0';
            meta_load_idx <= (others => '0');
            meta_p_time <= (others => '0');
        elsif( rising_edge(clock) ) then
            meta_fifo_read <= '0';
            if( meta_loaded = '0' ) then
                if( unsigned(meta_fifo_usedw) > 0) then
                    meta_p_time <= unsigned(meta_fifo_data(63 downto 0));
                    meta_loaded <= '1';
                    meta_fifo_read <= '1';
                end if;
            else
                if (meta_time_eq = '1') then
                    meta_loaded <= '0';
                end if;
            end if;
        end if;
    end process;
    meta_time_eq <= '1' when (enable = '1' and meta_fifo_empty = '0' and to_integer(timestamp(63 downto 0)) = to_integer(meta_p_time)) else '0';
    process(clock, reset)
    begin
        if (reset = '1') then
            meta_time_hit <= (others => '0');
        elsif(rising_edge(clock)) then
            if (meta_time_eq = '1') then
                meta_time_hit <= to_signed(1021, 16); --meta_time_hit'length);
            else
                if (meta_time_hit > 0) then
                    meta_time_hit <= meta_time_hit - 1;
                end if;
            end if;
        end if;
    end process;
    meta_time_go <= '1' when (meta_en = '1' and (meta_time_eq = '1' or meta_time_hit > 0 )) else '0';

    -- Assumes we want to read every other clock cycle
    read_fifo : process( clock, reset )
    begin
        if( reset = '1' ) then
            fifo_read <= '0' ;
        elsif( rising_edge( clock ) ) then
            fifo_read <= '0' ;
            if( enable = '1' ) then
                if( fifo_read = '0' and fifo_empty = '0' ) then
                    if (meta_en = '0' or (meta_en = '1' and meta_time_go = '1')) then
                        fifo_read <= '1' ;
                    end if;
                end if ;
            else
                fifo_read <= '0' ;
            end if ;
        end if ;
    end process ;

    -- Muxed values so empty reads come out as zeroes
    out_i <= resize(signed(fifo_data(11 downto  0)),out_i'length) when fifo_empty = '0' else (others =>'0') ;
    out_q <= resize(signed(fifo_data(27 downto 16)),out_q'length) when fifo_empty = '0' else (others =>'0') ;
    out_valid <= '0' when reset = '1' else
                 fifo_read when rising_edge(clock) ;

    -- Underflow detection
    detect_underflows : process( clock, reset )
    begin
        if( reset = '1' ) then
            underflow_detected <= '0' ;
        elsif( rising_edge( clock ) ) then
            underflow_detected <= '0' ;
            if( enable = '1' and fifo_empty = '1' and (meta_en = '0' or (meta_en = '1' and meta_time_go = '1')) ) then
                underflow_detected <= '1' ;
            end if ;
        end if ;
    end process ;

    -- Count the number of times we underflow, but only if they are discontinuous
    -- meaning we have an underflow condition, a non-underflow condition, then
    -- another underflow condition counts as 2 underflows, but an underflow condition
    -- followed by N underflow conditions counts as a single underflow condition.
    count_underflows : process( clock, reset )
        variable prev_underflow : std_logic := '0' ;
    begin
        if( reset = '1' ) then
            prev_underflow := '0' ;
            underflow_count <= (others =>'0') ;
        elsif( rising_edge( clock ) ) then
            if( prev_underflow = '0' and underflow_detected = '1' ) then
                underflow_count <= underflow_count + 1 ;
            end if ;
            prev_underflow := underflow_detected ;
        end if ;
    end process ;

    -- Active high assertion for underflow_duration when the underflow
    -- condition has been detected.  The LED will stay asserted
    -- if multiple underflows have occurred
    blink_underflow_led : process( clock, reset )
        variable downcount : natural range 0 to 2**underflow_duration'length-1 ;
    begin
        if( reset = '1' ) then
            downcount := 0 ;
            underflow_led <= '0' ;
        elsif( rising_edge(clock) ) then
            -- Default to not being asserted
            underflow_led <= '0' ;

            -- Countdown so we can see what happened
            if( downcount > 0 ) then
                downcount := downcount - 1 ;
                underflow_led <= '1' ;
            end if ;

            -- Underflow occurred so light it up
            if( underflow_detected = '1' ) then
                downcount := to_integer(underflow_duration) ;
            end if ;
        end if ;
    end process ;

end architecture ;

