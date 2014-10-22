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

entity fir_filter_tb is
end entity;

architecture arch of fir_filter_tb is

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

    --bit stripper signals
    signal strip_enable : std_logic;
    signal symbol_bits : std_logic_vector(2 downto 0);
    signal symbol_bits_request : std_logic;
    signal symbol_bits_valid : std_logic;

    --constellation mapper
    signal map_inputs : constellation_mapper_inputs_t;
    signal map_outputs : constellation_mapper_outputs_t;

    --new mixed signal
    signal symbol_mixed : complex_fixed_t;
    signal symbol_mixed_valid : std_logic;

    --new upsampled signal
    signal symbol_mixed_up : complex_fixed_t;
    signal symbol_mixed_up_valid : std_logic;

    --filtered signal
    signal filtered_sample : complex_fixed_t;
    signal filtered_sample_valid : std_logic;

    --filtered signal
    signal tx_symbol : complex_fixed_t;
    signal tx_symbol_valid : std_logic;

    --
    constant vsb_mix_table : complex_fixed_array_t := ( (to_signed(4096,16),    to_signed(0,16)),
                                                        (to_signed(0,16),       to_signed(-4096,16)),
                                                        (to_signed(-4096,16),   to_signed(0,16)),
                                                        (to_signed(0,16),       to_signed(4096,16) ));


    --

-- 4.0960e+03 - 0.0000e+00i
-- 3.5472e+03 - 2.0480e+03i
-- 2.0480e+03 - 3.5472e+03i
-- 2.5081e-13 - 4.0960e+03i
-- -2.0480e+03 - 3.5472e+03i
-- -3.5472e+03 - 2.0480e+03i
-- -4.0960e+03 - 5.0162e-13i
-- 3.5472e+03 + 2.0480e+03i
-- -2.0480e+03 + 3.5472e+03i
-- -7.5242e-13 + 4.0960e+03i
-- 2.0480e+03 + 3.5472e+03i
-- 3.5472e+03 + 2.0480e+03i
-- 4.0960e+03 + 1.0032e-12i

    constant atsc_lo_table : complex_fixed_array_t := ( (to_signed(4096,16),    to_signed(0,16)),
                                                        (to_signed(3547,16),       to_signed(-2048,16)),
                                                        (to_signed(2048,16),       to_signed(-3547,16)),
                                                        (to_signed(0,16),       to_signed(-4096,16)),
                                                        (to_signed(-2048,16),       to_signed(-3547,16)),
                                                        (to_signed(-3547,16),       to_signed(-2048,16)),
                                                        (to_signed(-4096,16),       to_signed(0,16)),
                                                        (to_signed(3547,16),       to_signed(2048,16)),
                                                        (to_signed(-2048,16),       to_signed(3547,16)),
                                                        (to_signed(0,16),       to_signed(4096,16)),
                                                        (to_signed(2048,16),       to_signed(3547,16)),
                                                        (to_signed(3547,16),       to_signed(4096,16) ));


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

    constant ATSC_FIR : real_array_t := (
        0.000952541947487,
        0.001021339440824,
        0.000713161320156,
        0.000099293834852,
        -0.000631484665308,
        -0.001227676654677,
        -0.001457043942307,
        -0.001190147219421,
        -0.000456941151589,
        0.000545581017437,
        0.001502219876242,
        0.002074087248050,
        0.002011614083297,
        0.001249974660528,
        -0.000046895490230,
        -0.001513860357388,
        -0.002682283457258,
        -0.003123186912762,
        -0.002593740652748,
        -0.001139501073282,
        0.000885746160816,
        0.002897996639881,
        0.004244432533851,
        0.004405493902086,
        0.003177417184785,
        0.000774939833928,
        -0.002187174361577,
        -0.004841259681936,
        -0.006310376881971,
        -0.005982741937804,
        -0.003734744853560,
        -0.000027621448548,
        0.004166072719588,
        0.007602382204908,
        0.009117175655313,
        0.007993866438765,
        0.004237744374945,
        -0.001336553068967,
        -0.007251920699642,
        -0.011732810079681,
        -0.013203082387958,
        -0.010786054755306,
        -0.004660238809862,
        0.003849830264059,
        0.012505380718689,
        0.018680222433748,
        0.020069392108675,
        0.015404316532634,
        0.004979659322029,
        -0.009171254616962,
        -0.023537317903158,
        -0.033804674326444,
        -0.035814465077870,
        -0.026588972738348,
        -0.005178629408275,
        0.026889883444145,
        0.065699566102954,
        0.105575674164579,
        0.140146665827966,
        0.163608592331722,
        0.171912865925582,
        0.163608592331722,
        0.140146665827966,
        0.105575674164579,
        0.065699566102954,
        0.026889883444145,
        -0.005178629408275,
        -0.026588972738348,
        -0.035814465077870,
        -0.033804674326444,
        -0.023537317903158,
        -0.009171254616962,
        0.004979659322029,
        0.015404316532634,
        0.020069392108675,
        0.018680222433748,
        0.012505380718689,
        0.003849830264059,
        -0.004660238809862,
        -0.010786054755306,
        -0.013203082387958,
        -0.011732810079681,
        -0.007251920699642,
        -0.001336553068967,
        0.004237744374945,
        0.007993866438765,
        0.009117175655313,
        0.007602382204908,
        0.004166072719588,
        -0.000027621448548,
        -0.003734744853560,
        -0.005982741937804,
        -0.006310376881971,
        -0.004841259681936,
        -0.002187174361577,
        0.000774939833928,
        0.003177417184785,
        0.004405493902086,
        0.004244432533851,
        0.002897996639881,
        0.000885746160816,
        -0.001139501073282,
        -0.002593740652748,
        -0.003123186912762,
        -0.002682283457258,
        -0.001513860357388,
        -0.000046895490230,
        0.001249974660528,
        0.002011614083297,
        0.002074087248050,
        0.001502219876242,
        0.000545581017437,
        -0.000456941151589,
        -0.001190147219421,
        -0.001457043942307,
        -0.001227676654677,
        -0.000631484665308,
        0.000099293834852,
        0.000713161320156,
        0.001021339440824,
        0.000952541947487);
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

        --setup nco input
        delta := to_signed(1,delta'length);
        nco_inputs.valid <= '0';
        nco_inputs.dphase <= (others => '0');

        nop(clock, 10);

        --setup state

        --release reset;
        reset <= '0';
        nop(clock, 10);

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

    feed_fifo: process
        variable in_data: unsigned(31 downto 0);
    begin
        strip_enable <= '0';
        tx_sample_fifo.wdata <= (others => '0');
        tx_sample_fifo.wreq <= '0';
        in_data := to_unsigned(0,in_data'length);
        nop(clock,10);

        nop(clock,10);


        for i in 0 to 10 loop
            tx_sample_fifo.wdata <= std_logic_vector(in_data);
            tx_sample_fifo.wreq <= '1';
            in_data := in_data + 1;

            wait until rising_edge(clock);
            tx_sample_fifo.wreq <= '0';
            nop(clock,10);
        end loop;

        strip_enable <= '1';

        for i in 0 to 1000 loop
            tx_sample_fifo.wdata <= std_logic_vector(in_data);
            tx_sample_fifo.wreq <= '1';
            in_data := in_data + 1;

            wait until rising_edge(clock);
            tx_sample_fifo.wreq <= '0';
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


    U_stripper : entity work.bit_stripper(arch)
       generic map(
           INPUT_WIDTH => 32,
           OUTPUT_WIDTH => 3
           )

        port map(
            clock => clock,
            reset => reset,

            enable => strip_enable,

            in_data => tx_sample_fifo.rdata,
            in_data_request => tx_sample_fifo.rreq,
            in_data_valid => '1',

            out_data => symbol_bits,
            out_data_request => symbol_bits_request,
            out_data_valid => symbol_bits_valid
        );

    --strip symbols
    pull_symbols : process(clock,reset)
        variable downcount : natural range 0 to 2;
    begin
        --
        if (reset = '1') then
            symbol_bits_request <= '0';
            downcount := 2;
        elsif rising_edge(clock) then

            if strip_enable = '1' then

                symbol_bits_request <= '0';
                if(downcount = 0) then
                    symbol_bits_request <= '1';
                    downcount := 2;
                else
                    downcount := downcount - 1 ;
                end if;
            end if;

        end if;
    end process;

    map_inputs.bits(map_inputs.bits'length -1 downto 3) <= (others => '0');
    map_inputs.bits(2 downto 0) <=  symbol_bits;
    map_inputs.modulation <= VSB_8;
    map_inputs.valid <= symbol_bits_valid;

    --map bits to constellation point
    U_mapper : entity work.constellation_mapper(arch)
        generic map(
            MODULATIONS => (VSB_8, MSK)
            )
        port map(
            clock => clock,

            inputs => map_inputs,
            outputs => map_outputs
        );

    generate_nco_phase : process(clock,reset)
        variable dphase : natural range 0 to 3;
    begin
        if (reset = '1') then
            dphase := 0;
            symbol_mixed_valid <= '0';
            symbol_mixed <= ((others=> '0'), (others => '0'));
        elsif (rising_edge(clock)) then
            symbol_mixed_valid <= '0';

            if map_outputs.valid = '1' then

                symbol_mixed <= cmult_fs4(map_outputs.symbol, dphase);
                symbol_mixed_valid <= '1';

                if dphase = vsb_mix_table'high then
                    dphase := 0;
                else
                    dphase := dphase + 1;
                end if;
            end if;
        end if;
    end process;

    --zero stuff to interpolate since this isn't available in the fir yet
    upsample : process(clock,reset)
        variable downcount : natural range 0 to 2;
    begin
        if (reset = '1') then
            downcount := 0;
            symbol_mixed_up_valid <= '0';
        elsif rising_edge(clock) then
            symbol_mixed_up_valid <= '0';
            if symbol_mixed_valid = '1' then
                symbol_mixed_up <= symbol_mixed;
                downcount := SPS-1;
                symbol_mixed_up_valid <= '1';
            elsif downcount > 0 then
                downcount := downcount -1;
                symbol_mixed_up <= ((others => '0'), (others => '0'));
                symbol_mixed_up_valid <= '1';
            end if;
        end if;
    end process;

    --filter to bandiwdth
    U_filter_re : entity work.fir_filter(systolic)
        generic map (
            CPS =>  1,
            H => ATSC_FIR
        )
        port map(
            clock => clock,
            reset => reset,

            in_sample => symbol_mixed_up.re,
            in_valid => symbol_mixed_up_valid,

            out_sample => filtered_sample.re,
            out_valid => filtered_sample_valid
        );


    U_filter_im : entity work.fir_filter(systolic)
        generic map (
            CPS =>  1,
            H => ATSC_FIR
        )
        port map(
            clock => clock,
            reset => reset,

            in_sample => symbol_mixed_up.im,
            in_valid => symbol_mixed_up_valid,

            out_sample => filtered_sample.im,
            out_valid => open
        );

    add_pilot : process(clock,reset)
        variable pilot : natural range atsc_lo_table'range;
    begin
        if(reset = '1') then
            tx_symbol <= ((others=>'0'),(others=>'0'));
            tx_symbol_valid <= '0';
        elsif rising_edge(clock) then
            tx_symbol_valid <= '0';

            if filtered_sample_valid = '1' then
                tx_symbol.re <= symbol_mixed.re + atsc_lo_table(pilot).re;
                tx_symbol.im <= symbol_mixed.im + atsc_lo_table(pilot).im;
                tx_symbol_valid <= '1';

                if pilot = atsc_lo_table'high then
                    pilot := 0;
                else
                    pilot := pilot +1;
                end if;
            end if;
        end if;
    end process;

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