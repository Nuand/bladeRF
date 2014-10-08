library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;


entity bit_stripper is
    generic(
        INPUT_WIDTH     : positive := 32;
        OUTPUT_WIDTH    : positive := 1;
        CPS             : positive := 2
    );

    port(
        clock : in std_logic;
        reset : in std_logic;

        enable : in std_logic;

        in_data : in std_logic_vector(INPUT_WIDTH-1 downto 0);
        in_data_request : out std_logic;
        in_data_valid : in std_logic;

        out_data : out std_logic_vector(OUTPUT_WIDTH-1 downto 0);
        out_data_request : in std_logic;
        out_data_valid : out std_logic
    );
end entity;

architecture arch of bit_stripper is

    signal data_buffer : std_logic_vector( (INPUT_WIDTH + OUTPUT_WIDTH)-1 downto 0);

    type stripper_fsm_t is (FLUSH,SHIFT_BUFFER,OUTPUT_DATA);
    signal stripper_fsm : stripper_fsm_t;

    constant THRESHOLD : positive := OUTPUT_WIDTH;


begin

    rip_bit : process(clock,reset)
        variable bit_count : natural range 0 to (INPUT_WIDTH + THRESHOLD + 1);
    begin
        if reset = '1' then
            data_buffer <= (others =>'0');
            bit_count := 0;
            stripper_fsm <= FLUSH;

            out_data <= (others => '0');
            out_data_valid <= '0';
            in_data_request <= '0';

        elsif rising_edge(clock) then

            out_data_valid <= '0';
            in_data_request <= '0';

            if enable = '0' then
                stripper_fsm <= FLUSH;
            end if;

            case stripper_fsm is
                when FLUSH =>
                    bit_count := 0;
                    data_buffer <= (others => '0');
                    if enable = '1' then
                        stripper_fsm <= SHIFT_BUFFER;
                    end if;

                when SHIFT_BUFFER =>
                        --
                        case bit_count is
                            when 0 => data_buffer <= "0000" & in_data;
                            when 1 => data_buffer <= "000" & in_data & data_buffer(0);
                            when 2 => data_buffer <= "00" & in_data & data_buffer(1 downto 0);
                            when 3 => data_buffer <= '0' & in_data & data_buffer(2 downto 0);
                            when 4 => data_buffer <= in_data & data_buffer(3 downto 0);
                            when others => data_buffer <=  "0000" & in_data;
                        end case;
                        bit_count := bit_count + INPUT_WIDTH;
                        stripper_fsm <= OUTPUT_DATA;
                        in_data_request <= '1';

                when OUTPUT_DATA =>

                    if out_data_request = '1' then
                        out_data <= data_buffer(OUTPUT_WIDTH -1 downto 0);
                        out_data_valid <= '1';
                        --shift the data down
                        data_buffer <= (OUTPUT_WIDTH-1 downto 0 => '0') & data_buffer( (INPUT_WIDTH+OUTPUT_WIDTH)-1 downto OUTPUT_WIDTH);
                        bit_count := bit_count - OUTPUT_WIDTH;

                    end if;

                    if bit_count <= THRESHOLD then
                        stripper_fsm <= SHIFT_BUFFER;
                    end if;


            end case;

        end if;
    end process;
end architecture;
