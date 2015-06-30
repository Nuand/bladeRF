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
        ADDR_WIDTH      :   positive := 7;   -- Number of address bits in an operation
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

    type command_t is ( READ, WRITE );

    type operation_t is record
        cmd     : command_t;
        addr    : std_logic_vector(ADDR_WIDTH-1 downto 0);
        data    : std_logic_vector(DATA_WIDTH-1 downto 0);
    end record;

    type operations_t is array(natural range<>) of operation_t;

    constant operations : operations_t := (
        (cmd => READ,  addr => 7x"55", data => (others => '0') ),
        (cmd => READ,  addr => 7x"7C", data => (others => '0') ),
        (cmd => WRITE, addr => 7x"27", data => 8x"42" ),
        (cmd => READ,  addr => 7x"27", data => (others => '0') )
    );

    type state_t is ( IDLE, RUN, HOLD );

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

                    if( mem_hold = '0' and clock_count >= operations'length*2*(ADDR_WIDTH+DATA_WIDTH)+10 ) then
                        stop(0);
                    end if;

                when RUN =>

                    clock_count := 0;

                    if( mem_hold = '0' ) then
                        case (operations(op).cmd) is
                            when READ  => rd_req <= '1';
                            when WRITE => wr_req <= '1';
                        end case;
                        addr    <= operations(op).addr;
                        wr_data <= operations(op).data;

                        if( op = operations'high ) then
                            state   <= IDLE;
                        else
                            op      := op + 1;
                            state   <= HOLD;
                        end if;
                    else
                        state <= HOLD;
                    end if;

                when HOLD =>

                    if( mem_hold = '0' and clock_count >= 64 ) then
                        state <= RUN;
                    end if;

            end case;

        end if;

    end process;

end architecture;
