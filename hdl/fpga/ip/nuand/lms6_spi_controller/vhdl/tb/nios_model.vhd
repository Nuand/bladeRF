--
--
-- !!! ======================== [ WARNING ] ======================== !!!
--
--          QUICK-AND-DIRTY TESTBENCH ... SCHEDULED FOR CLEAN UP
--
-- !!! ======================== [ WARNING ] ======================== !!!
--
--


library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

library std;
    use std.env.all;

entity nios_model is
    generic(
        ADDR_WIDTH      :   positive := 8;   -- Number of address bits in an operation
        DATA_WIDTH      :   positive := 8    -- Number of data bits in an operation
    );
    port(
        -- Avalon-MM Interface
        clock           :   out std_logic;
        reset           :   out std_logic;
        rd_req          :   out std_logic;
        wr_req          :   out std_logic;
        addr            :   out std_logic_vector(ADDR_WIDTH-1 downto 0);
        wr_data         :   out std_logic_vector(DATA_WIDTH-1 downto 0);
        rd_data         :   in  std_logic_vector(DATA_WIDTH-1 downto 0);
        rd_data_val     :   in  std_logic;
        mem_hold        :   in  std_logic
    );
end entity;

architecture arch of nios_model is

    function half_clk_per( freq : natural ) return time is
    begin
        return ( (0.5 sec) / real(freq) );
    end function;

    constant    SYS_CLK_PER : time      := half_clk_per( 80e6 );
    constant    SPI_CLK_DIV : positive  := 2;

    type command_t is ( READ, WRITE );

    type operation_t is record
        cmd     : command_t;
        addr    : std_logic_vector(ADDR_WIDTH-1 downto 0);
        data    : std_logic_vector(DATA_WIDTH-1 downto 0);
    end record;

    type operations_t is array(natural range<>) of operation_t;

    constant operations : operations_t := (
        (cmd => READ,  addr => 8x"55", data => (others => '1') ),
        (cmd => READ,  addr => 8x"7C", data => (others => '1') ),
        (cmd => WRITE, addr => 8x"27", data => 8x"42" ),
        (cmd => READ,  addr => 8x"27", data => 8x"42" ),
        (cmd => WRITE, addr => 8x"90", data => 8x"10" ),
        (cmd => WRITE, addr => 8x"91", data => 8x"11" ),
        (cmd => WRITE, addr => 8x"92", data => 8x"12" ),
        (cmd => WRITE, addr => 8x"93", data => 8x"13" ),
        (cmd => WRITE, addr => 8x"A0", data => 8x"20" ),
        (cmd => WRITE, addr => 8x"A1", data => 8x"21" ),
        (cmd => WRITE, addr => 8x"A2", data => 8x"22" ),
        (cmd => WRITE, addr => 8x"A3", data => 8x"23" ),
        (cmd => READ,  addr => 8x"10", data => 8x"10" ),
        (cmd => READ,  addr => 8x"11", data => 8x"11" ),
        (cmd => READ,  addr => 8x"12", data => 8x"12" ),
        (cmd => READ,  addr => 8x"13", data => 8x"13" ),
        (cmd => READ,  addr => 8x"20", data => 8x"20" ),
        (cmd => READ,  addr => 8x"21", data => 8x"21" ),
        (cmd => READ,  addr => 8x"22", data => 8x"22" ),
        (cmd => READ,  addr => 8x"23", data => 8x"23" )
    );

    type state_t is ( IDLE, RUN, CHECK_READ, NEXT_CMD );

    signal state    : state_t   := IDLE;

    signal sys_clk  : std_logic := '1';
    signal sys_rst  : std_logic := '1';

begin

    sys_clk <= not sys_clk after SYS_CLK_PER;
    sys_rst <= '1', '0' after SYS_CLK_PER*5;

    clock   <= sys_clk;
    reset   <= sys_rst;

    stim_proc : process( sys_clk, sys_rst )
        variable op : natural range operations'range := operations'low;
        variable clock_count : natural               := 0;
    begin
        if( sys_rst = '1' ) then
            op          := operations'low;
            clock_count := 0;
            state       <= RUN;
            rd_req      <= '0';
            wr_req      <= '0';
            addr        <= (others => '0');
            wr_data     <= (others => '0');
        elsif( rising_edge(sys_clk) ) then

            clock_count := clock_count + 1;

            rd_req      <= '0';
            wr_req      <= '0';
            addr        <= (others => '0');
            wr_data     <= (others => '0');

            case( state ) is

                when IDLE =>

                    if( mem_hold = '0' and clock_count >= operations'length*SPI_CLK_DIV*(ADDR_WIDTH+DATA_WIDTH)+10 ) then
                        stop(0);
                    end if;

                when RUN =>

                    clock_count := 0;

                    if( mem_hold = '0' ) then
                        case (operations(op).cmd) is
                            when READ  =>
                                rd_req <= '1';
                                state  <= CHECK_READ;
                            when WRITE =>
                                wr_req <= '1';
                                state  <= NEXT_CMD;
                        end case;

                        addr    <= operations(op).addr;
                        wr_data <= operations(op).data;

                    end if;

                when CHECK_READ =>

                    if( rd_data_val = '1' ) then
                        assert (operations(op).data = rd_data) report "Read data does not match expected!" severity failure;
                        state <= NEXT_CMD;
                    end if;

                when NEXT_CMD =>

                    if( mem_hold = '0' and clock_count >= 64 ) then
                        if( op = operations'high ) then
                            state <= IDLE;
                        else
                            op    := op + 1;
                            state <= RUN;
                        end if;
                    end if;

            end case;

        end if;

    end process;

end architecture;
