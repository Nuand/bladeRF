-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.log2;

-- -----------------------------------------------------------------------------
-- Entity:      lms6_spi_controller
-- Description: Avalon Memory-Mapped SPI controller for the LMS6002D.
-- Standard:    VHDL-2008
--
-- WARNING:     This module has not been tested with non-default generics.
-- -----------------------------------------------------------------------------
entity lms6_spi_controller is
    generic(
        CLOCK_DIV       :   positive  range 2 to 16 := 2;   -- Ratio of system clock to SPI clock
        ADDR_WIDTH      :   positive                := 8;   -- Number of address bits in an operation
        DATA_WIDTH      :   positive                := 8    -- Number of data bits in an operation
    );
    port(
        -- Physical Interface
        sclk            :   out std_logic;
        miso            :   in  std_logic;
        mosi            :   out std_logic;
        cs_n            :   out std_logic;

        -- Avalon-MM Interface
        mm_clock        :   in  std_logic;                                  -- System clock
        mm_reset        :   in  std_logic;                                  -- System reset
        mm_read         :   in  std_logic;                                  -- Initiates a SPI read operation
        mm_write        :   in  std_logic;                                  -- Initiates a SPI write operation
        mm_addr         :   in  std_logic_vector(ADDR_WIDTH-1 downto 0);    -- SPI address
        mm_din          :   in  std_logic_vector(DATA_WIDTH-1 downto 0);    -- SPI write data
        mm_dout         :   out std_logic_vector(DATA_WIDTH-1 downto 0);    -- SPI read data
        mm_dout_val     :   out std_logic;                                  -- Read data valid.
        mm_busy         :   out std_logic                                   -- Indicates the module is busy processing a command
    );
end entity;

architecture lms6 of lms6_spi_controller is

    constant    LMS_ADDR_WIDTH : positive := 7;
    constant    LMS_DATA_WIDTH : positive := 8;
    constant    NUM_PLL_REGS   : positive := 4;
    constant    MSG_LENGTH     : positive := 1 + LMS_ADDR_WIDTH + LMS_DATA_WIDTH;

    -- Special LMS PLL Register Addresses
    constant    TX0_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"90";
    constant    TX1_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"91";
    constant    TX2_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"92";
    constant    TX3_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"93";

    constant    RX0_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"A0";
    constant    RX1_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"A1";
    constant    RX2_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"A2";
    constant    RX3_ADDR  : std_logic_vector(ADDR_WIDTH-1 downto 0) := x"A3";

    -- FSM States
    type fsm_t is (
        IDLE,
        SHIFT
    );

    -- State of internal signals
    type state_t is record
        state           : fsm_t;
        clock_count     : unsigned(integer(log2(real(CLOCK_DIV)))-1 downto 0);
        shift_en        : std_logic;
        shift_count     : natural range 0 to MSG_LENGTH*NUM_PLL_REGS;
        shift_out_reg   : unsigned(MSG_LENGTH*NUM_PLL_REGS-1 downto 0);
        shift_in_reg    : unsigned(MSG_LENGTH-1 downto 0);
        sclk            : std_logic;
        cs_n            : std_logic;
        read_op         : std_logic;
        rd_data_valid   : std_logic;
    end record;

    constant reset_value : state_t := (
        state           => IDLE,
        clock_count     => (others => '0'),
        shift_en        => '0',
        shift_count     => 0,
        shift_out_reg   => (others => '0'),
        shift_in_reg    => (others => '0'),
        sclk            => '0',
        cs_n            => '1',
        read_op         => '0',
        rd_data_valid   => '0'
    );

    signal      current         : state_t;
    signal      future          : state_t;

begin

    sync_proc : process( mm_clock, mm_reset )
    begin
        if( mm_reset = '1' ) then
            current <= reset_value;
        elsif( rising_edge(mm_clock) ) then
            current <= future;
        end if;
    end process;

    comb_proc : process( all )
    begin

        -- Physical Outputs
        sclk <= current.sclk;
        cs_n <= current.cs_n;
        mosi <= current.shift_out_reg(state_t.shift_out_reg'left);

        -- Avalon-MM outputs
        mm_dout     <= std_logic_vector(current.shift_in_reg(LMS_DATA_WIDTH-1 downto 0));
        mm_dout_val <= current.rd_data_valid;
        mm_busy     <= '1';

        -- Next state defaults
        future                  <= current;
        future.sclk             <= '0';
        future.cs_n             <= '1';
        future.shift_en         <= '0';
        future.rd_data_valid    <= '0';

        case( current.state ) is

            when IDLE =>

                future.clock_count      <= (others => '0');
                future.read_op          <= mm_read;
                mm_busy                 <= '0';

                -- Take advantage of the multi-write capability of the LMS6 SPI
                -- interface. The only registers that we are going to support
                -- for now are the TX and RX NINT and NFRAC registers
                -- controlling the PLL settings. An extra address bit was added
                -- to the Avalon-MM interface and indicates that a multi-write
                -- message is intended. We queue up to NUM_PLL_REGS write
                -- transactions in the shift_out_reg. Upon detecting a write to
                -- the final register, the shift_count is set to the max.
                -- Otherwise, the default for a single message is used.
                case (mm_addr) is
                    when TX3_ADDR =>
                        -- Multi-message
                        future.shift_count <= MSG_LENGTH*NUM_PLL_REGS;
                    when RX3_ADDR =>
                        -- Multi-message
                        future.shift_count <= MSG_LENGTH*NUM_PLL_REGS;
                    when others =>
                        -- Single message
                        future.shift_count <= MSG_LENGTH;
                end case;

                if( (mm_read xor mm_write) = '1' ) then

                    future.shift_out_reg <= mm_write & unsigned(mm_addr(LMS_ADDR_WIDTH-1 downto 0)) & unsigned(mm_din)
                                            & current.shift_out_reg(current.shift_out_reg'left downto MSG_LENGTH);

                    -- Next state logic
                    case (mm_addr) is
                        when TX0_ADDR | TX1_ADDR | TX2_ADDR =>
                            future.state <= IDLE;
                        when RX0_ADDR | RX1_ADDR | RX2_ADDR =>
                            future.state <= IDLE;
                        when others =>
                            future.state <= SHIFT;
                    end case;
                end if;

            when SHIFT =>

                future.clock_count  <= current.clock_count + 1;
                future.cs_n         <= '0';
                future.sclk         <= current.clock_count(current.clock_count'left);

                if( current.clock_count = CLOCK_DIV-1 ) then
                    future.shift_en <= '1';
                else
                    future.shift_en <= '0';
                end if;

                if( current.shift_en = '1' ) then
                    future.shift_in_reg     <= current.shift_in_reg(current.shift_in_reg'left-1 downto current.shift_in_reg'right) & miso;
                    future.shift_out_reg    <= current.shift_out_reg(current.shift_out_reg'left-1 downto current.shift_out_reg'right) & '0';
                    future.shift_count      <= current.shift_count - 1;

                    if( current.shift_count = 1 ) then
                        -- Only assert read_valid if a read command was issued
                        future.rd_data_valid <= current.read_op;
                        future.state <= IDLE;
                    end if;
                end if;

        end case;

    end process;

end architecture;
