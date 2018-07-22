library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity fx3_gpif is
  port (

    -- FX3 control signals
    pclk            :   in  std_logic ;
    reset           :   in  std_logic ;

    -- USB Speed 0 = SS, 1 = HS
    usb_speed       :   in  std_logic ;

    -- FX3 GPIF interface
    gpif_in         :   in  std_logic_vector(31 downto 0) ;
    gpif_out        :   out std_logic_vector(31 downto 0) ;
    gpif_oe         :   out std_logic ;
    ctl_in          :   in  std_logic_vector(12 downto 0) ;
    ctl_out         :   out std_logic_vector(12 downto 0) ;
    ctl_oe          :   out std_logic_vector(12 downto 0) ;

    -- Enables
    tx_enable       :   out std_logic ;
    rx_enable       :   out std_logic ;
    meta_enable     :   in  std_logic ;

    -- TX FIFO
    tx_fifo_write   :   out std_logic ;
    tx_fifo_full    :   in  std_logic ;
    tx_fifo_empty   :   in  std_logic ;
    tx_fifo_usedw   :   in  std_logic_vector ;
    tx_fifo_data    :   out std_logic_vector(31 downto 0) ;

    -- TX meta FIFO
    tx_timestamp         :   in  unsigned(63 downto 0);
    tx_meta_fifo_write   :   out std_logic ;
    tx_meta_fifo_full    :   in  std_logic ;
    tx_meta_fifo_empty   :   in  std_logic ;
    tx_meta_fifo_usedw   :   in  std_logic_vector ;
    tx_meta_fifo_data    :   out std_logic_vector(31 downto 0) ;

    -- RX FIFO
    rx_fifo_read    :   out std_logic ;
    rx_fifo_full    :   in  std_logic ;
    rx_fifo_empty   :   in  std_logic ;
    rx_fifo_usedw   :   in  std_logic_vector ;
    rx_fifo_data    :   in  std_logic_vector(31 downto 0) ;

    -- RX meta FIFO
    rx_meta_fifo_read    :   out std_logic ;
    rx_meta_fifo_full    :   in  std_logic ;
    rx_meta_fifo_empty   :   in  std_logic ;
    rx_meta_fifo_usedr   :   in  std_logic_vector ;
    rx_meta_fifo_data    :   in  std_logic_vector(31 downto 0)
  ) ;
end entity ;

architecture sample_shuffler of fx3_gpif is

    -- Control mapping
    alias dma0_rx_ack   is ctl_out(0) ;
    alias dma1_rx_ack   is ctl_out(1) ;
    alias dma2_tx_ack   is ctl_out(2) ;
    alias dma3_tx_ack   is ctl_out(3) ;
    alias dma_rx_enable is ctl_in(4) ;
    alias dma_tx_enable is ctl_in(5) ;
    --alias dma_idle      is ctl_in(6) ;
    alias dma0_rx_reqx  is ctl_in(8) ;
    alias dma1_rx_reqx  is ctl_in(12) ; -- due to 9 being connected to dclk
    alias dma2_tx_reqx  is ctl_in(10) ;
    alias dma3_tx_reqx  is ctl_in(11) ;

    constant CONTROL_OE : std_logic_vector := "0000000001111" ;

    signal dma_idle     :   std_logic ;

    signal can_rx       :   std_logic ;
    signal can_tx       :   std_logic ;
    signal should_rx    :   std_logic ;
    signal should_tx    :   std_logic ;

    type dma_event is (DE_TX, DE_RX);
    signal dma_last_event : dma_event;
    type state_t is (IDLE, IDLE_RD, IDLE_RD_1, IDLE_WR, IDLE_WR_1, IDLE_WR_2, IDLE_WR_3, META_READ, SAMPLE_READ, META_WRITE, SAMPLE_WRITE, SAMPLE_WRITE_IGNORE, FINISHED);
    signal state : state_t;

    signal gpif_buf_size        :   unsigned(12 downto 0) ;
    signal gpif_buf_size_cond   :   signed(12 downto 0) ;

    signal tx_fifo_enough : std_logic ;
    signal rx_fifo_enough : std_logic ;

    signal rx_next_dma : std_logic ;
    signal tx_next_dma : std_logic ;

    signal tx_meta_en : std_logic ;
    signal rx_meta_en : std_logic ;

    signal dma_downcount : signed(12 downto 0) ;
    signal meta_downcount : signed(12 downto 0) ;

    signal underrun : std_logic;
    signal underrun_set, underrun_set_r : std_logic;
    signal underrun_clr, underrun_clr_r : std_logic;


    signal meta_buffer : std_logic_vector(127 downto 0);

    attribute preserve: boolean;
    attribute preserve of can_rx: signal is true;
    attribute preserve of can_tx: signal is true;
    attribute preserve of should_rx: signal is true;
    attribute preserve of should_tx: signal is true;

    attribute keep: boolean;
    attribute keep of can_rx: signal is true;
    attribute keep of can_tx: signal is true;
    attribute keep of should_rx: signal is true;
    attribute keep of should_tx: signal is true;


begin

    process ( reset, pclk )
    begin
        if (reset = '1') then
            underrun <= '0';
            underrun_set_r <= '0';
            underrun_clr_r <= '0';
        elsif (rising_edge(pclk)) then
            underrun_set_r <= underrun_set;
            underrun_clr_r <= underrun_clr;
            if (underrun_set_r = '0' and underrun_set = '1') then
                underrun <= '1';
            elsif(underrun_clr_r = '0' and underrun_clr = '1') then
                underrun <= '0';
            end if;
        end if;
    end process;

    tx_meta_en <= meta_enable;
    rx_meta_en <= meta_enable;

    -- Unused outputs
    ctl_out(12 downto 4) <= (others =>'0') ;

    -- Enable signals going out
    rx_enable <= '0' when reset = '1' else dma_rx_enable when rising_edge(pclk) ;
    tx_enable <= '0' when reset = '1' else dma_tx_enable when rising_edge(pclk) ;

    -- Constant drivers
    ctl_oe <= CONTROL_OE ;

    -- Transfer size for DMA's

    calculate_conditionals : process( reset, pclk )
    begin
        if( reset = '1' ) then
            gpif_buf_size <= to_unsigned(512, gpif_buf_size'length) ;
            gpif_buf_size_cond <= to_signed(511, gpif_buf_size_cond'length) ;
        elsif( rising_edge(pclk) ) then
            if( usb_speed = '0' ) then
                gpif_buf_size <= to_unsigned(512, gpif_buf_size'length) ;
                gpif_buf_size_cond <= to_signed(511, gpif_buf_size_cond'length) ;
            else
                gpif_buf_size <= to_unsigned(256, gpif_buf_size'length) ;
                gpif_buf_size_cond <= to_signed(255, gpif_buf_size_cond'length) ;
            end if ;

            if( unsigned(tx_fifo_usedw) < ((2**(tx_fifo_usedw'length-1) - gpif_buf_size)) ) then
                tx_fifo_enough <= '1' ;
            else
                tx_fifo_enough <= '0' ;
            end if ;

            if( unsigned(rx_fifo_full&rx_fifo_usedw) > gpif_buf_size ) then
                rx_fifo_enough <= '1' ;
            else
                rx_fifo_enough <= '0' ;
            end if ;
        end if ;
    end process ;

    rx_fifo_read  <= '1' when ((rx_meta_en = '0' and state = IDLE_RD) or state = SAMPLE_READ ) else '0';
    tx_fifo_write <= '1' when (state = SAMPLE_WRITE) else '0';
    tx_meta_fifo_write <= '1' when (state = SAMPLE_WRITE and meta_enable = '1' and meta_downcount >= 0) else '0';
    rx_meta_fifo_read <= '1' when (state = META_READ or (rx_meta_en = '1' and state = IDLE_RD) ) else '0';

    process(all)
    begin
        if( state = SAMPLE_READ or (rx_meta_en = '0' and state = IDLE_RD) or state = IDLE_RD_1 or state = FINISHED ) then
            gpif_out <= rx_fifo_data ;
        elsif( state = META_READ or (rx_meta_en = '1' and state = IDLE_RD) ) then
            if (meta_downcount = 0) then
                gpif_out <= rx_meta_fifo_data(31 downto 16) & "00000000" & "0000" & (not underrun) & (not underrun) & underrun & underrun;
            else
                gpif_out <= rx_meta_fifo_data;
            end if;
        else
            gpif_out <= (others =>'0');
        end if ;

        if( state = IDLE_WR or state = IDLE_WR_1 or state = IDLE_WR_2 or state = IDLE_WR_3 or state = SAMPLE_WRITE or state = FINISHED ) then
            tx_fifo_data <= gpif_in;
        else
            tx_fifo_data <= (others =>'0');
        end if;

        if( state = SAMPLE_WRITE and meta_enable = '1') then
            tx_meta_fifo_data <= meta_buffer(127 downto 96);
        else
            tx_meta_fifo_data <= (others => '0');
        end if;

    end process;

    can_and_should : process( pclk, reset )
    begin
        if( reset = '1' ) then
            can_rx <= '0' ;
            can_tx <= '0' ;
            should_rx <= '0' ;
            should_tx <= '0' ;
            dma_idle <= '0' ;
        elsif( rising_edge(pclk) ) then
            dma_idle <= ctl_in(6) ;
            if( dma_rx_enable = '1' and rx_fifo_enough = '1' and (dma0_rx_reqx = '0' or dma1_rx_reqx = '0') ) then
                can_rx <= '1' ;
            else
                can_rx <= '0' ;
            end if ;

            if( dma_tx_enable = '1' and tx_fifo_enough = '1' and (dma2_tx_reqx = '0' or dma3_tx_reqx = '0' ) ) then
                can_tx <= '1' ;
            else
                can_tx <= '0' ;
            end if ;

            if( can_rx = '1' ) then
                if( can_tx = '0' ) then
                    should_rx <= '1' ;
--                elsif( dma_last_event = DE_TX ) then
--                    should_rx <= '1' ;
--                else
--                    should_rx <= '0' ;
                else
                    should_rx <= '1';
                end if ;
            else
                should_rx <= '0' ;
            end if ;

            if( can_tx = '1' ) then
                should_tx <= '1';
--                if( can_rx = '0' ) then
--                    should_tx <= '1' ;
--                elsif( dma_last_event = DE_RX ) then
--                    should_tx <= '1' ;
--                else
--                    should_tx <= '0' ;
--                end if ;
            else
                should_tx <= '0' ;
            end if ;
        end if ;
    end process ;

    process(reset, pclk)
    begin
        if( reset = '1' ) then
            state <= IDLE;
            tx_next_dma <= '1';
            rx_next_dma <= '0';
            dma0_rx_ack <= '0';
            dma1_rx_ack <= '0';
            dma2_tx_ack <= '0';
            dma3_tx_ack <= '0';
            dma_downcount <= (others => '0');
            dma_last_event <= DE_TX;
            meta_downcount <= (others => '0');
            gpif_oe <= '1';
            underrun_set <= '0';
            underrun_clr <= '0';
        elsif( rising_edge(pclk) ) then
            case state is
                when IDLE =>
                    underrun_set <= '0';
                    underrun_clr <= '0';
                    if( dma_idle = '1' ) then
                        if( should_rx = '1' ) then
                            dma_downcount <= gpif_buf_size_cond - 2;

                            if ( rx_next_dma = '0') then
                                dma0_rx_ack <= '1';
                                dma1_rx_ack <= '0';
                            else
                                dma0_rx_ack <= '0';
                                dma1_rx_ack <= '1';
                            end if;

                            rx_next_dma <= '0';

                            state <= IDLE_RD;
                            dma_last_event <= DE_RX;
                        elsif( should_tx = '1' ) then
                            dma_downcount <= gpif_buf_size_cond;

                            if( tx_next_dma = '0') then
                                dma2_tx_ack <= '1';
                                dma3_tx_ack <= '0';
                            else
                                dma2_tx_ack <= '0';
                                dma3_tx_ack <= '1';
                            end if;

                            tx_next_dma <= '1';

                            state <= IDLE_WR;
                            dma_last_event <= DE_TX;
                        end if;
                        meta_downcount <= to_signed(3, meta_downcount'length);
                    end if;

                when IDLE_WR =>
                    state <= IDLE_WR_1;
                    gpif_oe <= '0' ;
                when IDLE_WR_1 =>
                    state <= IDLE_WR_2;
                when IDLE_WR_2 =>
                    state <= IDLE_WR_3;
                when IDLE_WR_3 =>
                    if( tx_meta_en = '1' ) then
                        state <= META_WRITE;
                        meta_buffer <= (others => '0');
                    else
                        state <= SAMPLE_WRITE;
                    end if;
                when META_WRITE =>
                    meta_buffer(127 downto 0) <= meta_buffer(95 downto 0) & gpif_in(31 downto 0);
                    if( meta_downcount > 0 ) then
                        dma_downcount <= dma_downcount - 1;
                        meta_downcount <= meta_downcount - 1;
                    else
                        dma_downcount <= dma_downcount - 1;
                        if (unsigned(meta_buffer(63 downto 0)) = 0 or unsigned(meta_buffer(31 downto 0) & meta_buffer(63 downto 32)) > (tx_timestamp + 32)) then
                           meta_downcount <= to_signed(3, 13);
                           state <= SAMPLE_WRITE;
                        else
                           state <= SAMPLE_WRITE_IGNORE;
                        end if;
                    end if;
                when SAMPLE_WRITE_IGNORE =>
                    dma2_tx_ack <= '0';
                    dma3_tx_ack <= '0';
                    underrun_set <= '1';
                    if( dma_downcount > 0 ) then
                        dma_downcount <= dma_downcount - 1;
                    else
                        state <= FINISHED;
                    end if;
                when SAMPLE_WRITE =>
                    if( meta_downcount >= 0 ) then
                        meta_downcount <= meta_downcount - 1;
                        meta_buffer(127 downto 0) <= meta_buffer(95 downto 0) & x"00000000";
                    end if;
                    dma2_tx_ack <= '0';
                    dma3_tx_ack <= '0';
                    if( dma_downcount > 0 ) then
                        dma_downcount <= dma_downcount - 1;
                    else
                        state <= FINISHED;
                    end if;
                when IDLE_RD =>
                    meta_downcount <= meta_downcount - 1;
                    if( rx_meta_en = '1') then
                        state <= META_READ;
                    else
                        state <= SAMPLE_READ ;
                    end if;
                when IDLE_RD_1 =>
                    if( rx_meta_en = '1') then
                        state <= META_READ;
                    else
                        state <= SAMPLE_READ ;
                    end if;
                when META_READ =>
                    if( meta_downcount > 0) then
                        dma_downcount <= dma_downcount - 1;
                        meta_downcount <= meta_downcount - 1;
                    else
                        dma_downcount <= dma_downcount - 1;
                        state <= SAMPLE_READ;
                    end if;
                when SAMPLE_READ =>
                    underrun_clr <= '1';
                    dma0_rx_ack <= '0';
                    dma1_rx_ack <= '0';

                    if( dma_downcount >= 0 ) then
                        dma_downcount <= dma_downcount - 1;
                    else
                        state <= FINISHED;
                        dma_downcount <= to_signed(10, dma_downcount'length) ;
                    end if;
                when FINISHED =>
                    if( dma_downcount >= 0 ) then
                        dma_downcount <= dma_downcount - 1 ;
                    else
                        state <= IDLE ;
                        gpif_oe <= '1' ;
                    end if ;
            end case;

        end if;
    end process;


end architecture ;

