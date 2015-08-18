library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity spi_master_tb is
end entity;

architecture tb of spi_master_tb is

    constant    CLOCK_DIV       :   positive  range 2 to 16 := 2;   -- Ratio of system clock to SPI clock
    constant    ADDR_WIDTH      :   positive                := 8;   -- Number of address bits in an operation
    constant    DATA_WIDTH      :   positive                := 8;   -- Number of data bits in an operation

    signal      clock           :   std_logic;
    signal      reset           :   std_logic;
    signal      rd_req          :   std_logic;
    signal      wr_req          :   std_logic;
    signal      addr            :   std_logic_vector(ADDR_WIDTH-1 downto 0);
    signal      wr_data         :   std_logic_vector(DATA_WIDTH-1 downto 0);
    signal      rd_data         :   std_logic_vector(DATA_WIDTH-1 downto 0);
    signal      rd_data_val     :   std_logic;
    signal      mem_hold        :   std_logic;

    signal      sclk            :   std_logic;
    signal      miso            :   std_logic;
    signal      mosi            :   std_logic;
    signal      cs_n            :   std_logic;

begin

    nios : entity work.nios_model
    generic map(
        ADDR_WIDTH      => ADDR_WIDTH,
        DATA_WIDTH      => DATA_WIDTH
    )
    port map(
        -- Avalon-MM Interface
        clock           => clock,           -- out std_logic;
        reset           => reset,           -- out std_logic;
        rd_req          => rd_req,          -- out std_logic
        wr_req          => wr_req,          -- out std_logic
        addr            => addr,            -- out std_logic_vector(ADDR_WIDTH-1 downto 0);
        wr_data         => wr_data,         -- out std_logic_vector(DATA_WIDTH-1 downto 0);
        rd_data         => rd_data,         -- in  std_logic_vector(DATA_WIDTH-1 downto 0);
        rd_data_val     => rd_data_val,     -- in  std_logic
        mem_hold        => mem_hold         -- in  std_logic
    );

    uut : entity work.lms6_spi_controller(lms6)
    generic map(
        CLOCK_DIV       => CLOCK_DIV,
        ADDR_WIDTH      => ADDR_WIDTH,
        DATA_WIDTH      => DATA_WIDTH
    )
    port map(
        -- Physical Interface
        sclk            => sclk,            -- out std_logic;
        miso            => miso,            -- in  std_logic;
        mosi            => mosi,            -- out std_logic;
        cs_n            => cs_n,            -- out std_logic;

        -- Avalon-MM Interface
        mm_clock        => clock,           -- in  std_logic;
        mm_reset        => reset,           -- in  std_logic;
        mm_read         => rd_req,          -- in  std_logic;
        mm_write        => wr_req,          -- in  std_logic;
        mm_addr         => addr,            -- in  std_logic_vector(ADDR_WIDTH-1 downto 0);
        mm_din          => wr_data,         -- in  std_logic_vector(DATA_WIDTH-1 downto 0);
        mm_dout         => rd_data,         -- out std_logic_vector(DATA_WIDTH-1 downto 0);
        mm_dout_val     => rd_data_val,     -- out std_logic;
        mm_busy         => mem_hold         -- out std_logic;
    );

    lms : entity work.lms6002d_model
    port map (
        -- LMS RX Interface
        rx_clock        => '0',             -- in      std_logic;
        rx_clock_out    => open,            -- buffer  std_logic;
        rx_data         => open,            -- buffer  signed(11 downto 0);
        rx_enable       => '0',             -- in      std_logic;
        rx_iq_select    => open,            -- buffer  std_logic;

        -- LMS TX Interface
        tx_clock        => '0',             -- in      std_logic;
        tx_data         => (others => '0'), -- in      signed(11 downto 0);
        tx_enable       => '0',             -- in      std_logic;
        tx_iq_select    => '0',             -- in      std_logic;

        -- LMS SPI Interface
        sclk            => sclk,            -- in      std_logic;
        sen             => cs_n,            -- in      std_logic;
        sdio            => mosi,            -- in      std_logic;
        sdo             => miso,            -- buffer  std_logic;

        -- LMS Control Interface
        pll_out         => open,            -- buffer  std_logic;
        resetx          => '0'              -- in      std_logic
    );

end architecture;
