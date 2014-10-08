library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;
    use ieee.math_complex.all;

library std;
    use std.env.all;

library work;
    use work.nco_p.all;

library nuand;
    use nuand.util.all;
    use nuand.constellation_mapper_p.all;

entity atsc_tx_tb is
end entity;

architecture arch of atsc_tx_tb is

    signal clock : std_logic := '1';
    signal reset : std_logic := '1';

    signal nco_inputs : nco_input_t;
    signal nco_outputs : nco_output_t;


    --fifo management
    type fifo_t is record
        aclr    :   std_logic ;

        wclock  :   std_logic ;
        wdata   :   std_logic_vector(31 downto 0) ;
        wreq    :   std_logic ;
        wempty  :   std_logic ;
        wfull   :   std_logic ;
        wused   :   std_logic_vector(11 downto 0) ;

        rclock  :   std_logic ;
        rdata   :   std_logic_vector(31 downto 0) ;
        rreq    :   std_logic ;
        rempty  :   std_logic ;
        rfull   :   std_logic ;
        rused   :   std_logic_vector(11 downto 0) ;
    end record ;

    signal tx_sample_fifo :fifo_t;

    signal tx_enable : std_logic;
    signal tx_valid : std_logic;

    --filtered signal
    signal tx_symbol : complex_fixed_t;
    signal tx_symbol_valid : std_logic;

    --
    constant vsb_mix_table : complex_fixed_array_t := ( (to_signed(4096,16),    to_signed(0,16)),
                                                        (to_signed(0,16),       to_signed(-4096,16)),
                                                        (to_signed(-4096,16),   to_signed(0,16)),
                                                        (to_signed(0,16),       to_signed(4096,16) ));

    function cmult( a, b : complex_fixed_t; q : natural ) return complex_fixed_t is
        variable rv : complex_fixed_t := (to_signed(0,16),to_signed(0,16));
    begin
        rv.re := resize(shift_right(a.re * b.re,q) - shift_right(a.im * b.im, q),rv.re'length);
        rv.re := resize(shift_right(a.re * b.re,q) + shift_right(a.im * b.re, q),rv.im'length);
        return rv;
    end function;

    function cmult_fs4(a : complex_fixed_t; fs_4 : natural) return complex_fixed_t is
    begin
        case fs_4 is
            when 0 => return (a.re,a.im);
            when 1 => return (a.im,-a.re);
            when 2 => return (-a.re,-a.im);
            when 3 => return (-a.im, a.re);
            when others => return a;
        end case;
    end function;



    procedure nop( signal clock : in std_logic; count : in natural) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock);
        end loop;
    end procedure;

    constant SPS : natural := 3;

    constant RANDOM_MULT : unsigned(63 downto 0) := x"2545f4914f6cdd1d";

    function random_xor (old : unsigned) return unsigned is
        variable x : unsigned(old'range) := (others=>'0');
        variable y : unsigned(old'range) := (others=>'0');
        variable z : unsigned(old'range) := (others=>'0');
    begin
        x := old xor shift_right(old,12);
        y := x xor shift_left(x,25);
        z := y xor shift_right(y,27);
        return resize( shift_right(z * RANDOM_MULT,z'length),z'length);
    end function;

    signal data_request_reader : std_logic;
    signal delay_request_reader : std_logic;
begin

    clock <= not clock after 1 ns;

    --construct fir filter under test
    --U_filter : entity work.fir_filter(systolic)
    --    generic map()
    --    port map();

    tb : process
        variable delta : signed(15 downto 0);

    begin
        reset <= '1';
        delay_request_reader <='0';
        --setup nco input
        delta := to_signed(1,delta'length);
        nco_inputs.valid <= '0';
        nco_inputs.dphase <= (others => '0');

        nop(clock, 10);

        --setup state

        --release reset;
        reset <= '0';
        nop(clock, 10);

        delay_request_reader <='1';

        --clock in data
        for i in 0 to 100000 loop



            if (nco_inputs.dphase = 4096) then
                delta := to_signed(-1,delta'length);
            elsif(nco_inputs.dphase = -4096) then
                delta := to_signed(1,delta'length);
            end if;

            nco_inputs.dphase <= nco_inputs.dphase + delta;
            nco_inputs.valid <= '1' ;
            wait until rising_edge(clock);
            nco_inputs.valid <= '0';

            nop(clock,10);
        end loop;
        --save result


    end process;

    data_request_reader <= '1' when ((unsigned(tx_sample_fifo.wused) < to_unsigned(2048,tx_sample_fifo.wused'length)) and delay_request_reader ='1' )else '0';

    U_file_reader : entity work.data_reader
        generic map (
            FILENAME => "input.dat",
            DATA_WIDTH => 32
        )
        port map (
            reset => reset,
            clock => clock,

            data_request => data_request_reader,
            data => tx_sample_fifo.wdata,
            data_valid => tx_sample_fifo.wreq
        );


    feed_fifo: process
        variable in_data: unsigned(31 downto 0) := x"1337cafe";
    begin
        tx_enable <= '0';
        --tx_sample_fifo.wdata <= (others => '0');
        --tx_sample_fifo.wreq <= '0';
        nop(clock,10);



        for i in 0 to 10 loop
            --tx_sample_fifo.wdata <= std_logic_vector(in_data);
            --tx_sample_fifo.wreq <= '1';
            in_data :=random_xor(in_data);

            wait until rising_edge(clock);
            --tx_sample_fifo.wreq <= '0';
            nop(clock,10);
        end loop;

        tx_enable <= '1';

        for i in 0 to 100000 loop
            --tx_sample_fifo.wdata <= std_logic_vector(in_data);
            --tx_sample_fifo.wreq <= '1';
            in_data :=random_xor(in_data);

            wait until rising_edge(clock);
            --tx_sample_fifo.wreq <= '0';
            nop(clock,20);
        end loop;
    end process;

    --tmp
    tx_sample_fifo.aclr <= reset ;
    tx_sample_fifo.rclock <= clock ;
    tx_sample_fifo.wclock <= clock ;

    U_bit_fifo : entity work.tx_fifo
      port map (
        aclr                => tx_sample_fifo.aclr,
        data                => tx_sample_fifo.wdata,
        rdclk               => tx_sample_fifo.rclock,
        rdreq               => tx_sample_fifo.rreq,
        wrclk               => tx_sample_fifo.wclock,
        wrreq               => tx_sample_fifo.wreq,
        q                   => tx_sample_fifo.rdata,
        rdempty             => tx_sample_fifo.rempty,
        rdfull              => tx_sample_fifo.rfull,
        rdusedw             => tx_sample_fifo.rused,
        wrempty             => tx_sample_fifo.wempty,
        wrfull              => tx_sample_fifo.wfull,
        wrusedw             => tx_sample_fifo.wused
        );

    tx_valid_register : process(reset, clock)
    begin
        if (reset = '1') then
            tx_valid <= '0';
        elsif rising_edge(clock) then
            tx_valid <= tx_sample_fifo.rreq;
        end if;
    end process;

    U_atsc_transmitter : entity work.atsc_tx(arch)
        generic map(
            INPUT_WIDTH => tx_sample_fifo.rdata'length,
            OUTPUT_WIDTH => tx_symbol.re'length
        )
        port map (
        reset => reset,
        clock => clock,

        tx_enable           => tx_enable,
        data_in             => tx_sample_fifo.rdata,
        data_in_request     => tx_sample_fifo.rreq,
        data_in_valid       => tx_valid,

        sample_out_i        => tx_symbol.re,
        sample_out_q        => tx_symbol.im,
        sample_out_valid    => tx_symbol_valid

    );


    U_data_saver_im : entity work.signed_saver(arch)
        generic map (
            FILENAME => "tx_sym.im"
        )
        port map(
            clock => clock,
            reset => reset,
            data => (tx_symbol.im),
            data_valid => tx_symbol_valid
        );

    U_data_saver_re : entity work.signed_saver(arch)
        generic map (
            FILENAME => "tx_sym.re"
        )
        port map(
            clock => clock,
            reset => reset,
            data => (tx_symbol.re),
            data_valid => tx_symbol_valid
        );

   --U_nco_test : entity work.nco(arch)
   --    port map (
   --        clock => clock,
   --        reset => reset,

   --        inputs => nco_inputs,
   --        outputs => nco_outputs
   --    );


end architecture;