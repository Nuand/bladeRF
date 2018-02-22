library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

--library work;
--    use work.nco_p.all;

library nuand;
--   use nuand.util.all;
    use nuand.constellation_mapper_p.all;

entity atsc_tx is
    generic(
        INPUT_WIDTH : positive := 32;
        OUTPUT_WIDTH : positive:= 16;
        SYMBOL_DOWNLOAD_WIDTH : positive := 4;
        SPS : natural := 3;
        CPS : natural := 2
    );
    port(
        clock : in std_logic;
        reset : in std_logic;

       --
        tx_enable : in std_logic;

        data_in : in std_logic_vector(INPUT_WIDTH-1 downto 0);
        data_in_request : out std_logic;
        data_in_valid : in std_logic;

        sample_out_i : out signed(OUTPUT_WIDTH-1 downto 0);
        sample_out_q : out signed(OUTPUT_WIDTH-1 downto 0);
        sample_out_valid : out std_logic
    );
end entity;


architecture arch of atsc_tx is

    --bit stripper signals
    signal strip_enable : std_logic;
    signal symbol_bits : std_logic_vector(SYMBOL_DOWNLOAD_WIDTH-1 downto 0);
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

    --filtered signal
    signal registered_symbol : complex_fixed_t;
    signal registered_symbol_valid : std_logic;

    --pilot
    signal tx_pilot : complex_fixed_t;

    -- todo:create tone dynamically
    function create_tone( fs, ftone : real) return complex_fixed_t is
        variable rv : complex_fixed_t := (to_signed(0,16),to_signed(0,16));
    begin
        return rv;
    end function;


    constant atsc_lo_table : complex_fixed_array_t := ( (to_signed( integer(4096.0*1.5/7.0),16),        to_signed( integer(1.5/7.0 * 0.0),16)),
                                                        (to_signed( integer(1.5/7.0 * 3547.0),16),      to_signed( integer(1.5/7.0 * (-2048.0)),16)),
                                                        (to_signed( integer(1.5/7.0 * 2048.0),16),      to_signed( integer(1.5/7.0 * (-3547.0)),16)),
                                                        (to_signed( integer(1.5/7.0 * 0.0),16),         to_signed( integer(1.5/7.0 * (-4096.0)),16)),
                                                        (to_signed( integer(1.5/7.0 * (-2048.0)),16),   to_signed( integer(1.5/7.0 * (-3547.0)),16)),
                                                        (to_signed( integer(1.5/7.0 * (-3547.0)),16),   to_signed( integer(1.5/7.0 * (-2048.0)),16)),
                                                        (to_signed( integer(1.5/7.0 * (-4096.0)),16),   to_signed( integer(1.5/7.0 * 0.0),16)),
                                                        (to_signed( integer(1.5/7.0 * (-3547.0)),16),   to_signed( integer(1.5/7.0 * 2048.0),16)),
                                                        (to_signed( integer(1.5/7.0 * (-2048.0)),16),   to_signed( integer(1.5/7.0 * 3547.0),16)),
                                                        (to_signed( integer(1.5/7.0 * 0.0),16),         to_signed( integer(1.5/7.0 * 4096.0),16)),
                                                        (to_signed( integer(1.5/7.0 * 2048.0),16),      to_signed( integer(1.5/7.0 * 3547.0),16)),
                                                        (to_signed( integer(1.5/7.0 * 3547.0),16),      to_signed( integer(1.5/7.0 * 2048.0),16) ));



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


    --map outputs
    sample_out_i <= registered_symbol.re;
    sample_out_q <= registered_symbol.im;
    sample_out_valid <= registered_symbol_valid;

    strip_enable <= tx_enable;

    U_stripper : entity work.bit_stripper(arch)
       generic map(
           INPUT_WIDTH => 32,
           OUTPUT_WIDTH => 4
           )

        port map(
            clock => clock,
            reset => reset,

            enable => strip_enable,

            in_data => data_in,
            in_data_request => data_in_request,
            in_data_valid => data_in_valid,

            out_data => symbol_bits,
            out_data_request => symbol_bits_request,
            out_data_valid => symbol_bits_valid
        );

    --strip symbols
    pull_symbols : process(clock,reset)
        constant DOWNCOUNT_RESET : natural range 0 to 5 := 2;
        variable downcount       : natural range 0 to 5 := DOWNCOUNT_RESET;
    begin
       --
        if (reset = '1') then
            symbol_bits_request <= '0';
            downcount := DOWNCOUNT_RESET;
        elsif rising_edge(clock) then

            if strip_enable = '1' then

                symbol_bits_request <= '0';
                if(downcount = 0) then
                    symbol_bits_request <= '1';
                    downcount := 5;
                else
                    downcount := downcount - 1 ;
                end if;
            end if;

        end if;
    end process;

    map_inputs.bits(map_inputs.bits'length -1 downto 3) <= (others => '0');
    map_inputs.bits(2 downto 0) <=  symbol_bits(2 downto 0);
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

    --apply fs/4 multiply to center
    generate_nco_phase : process(clock,reset)
        subtype  phases_t is natural range 0 to 3;
        variable dphase : phases_t;
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

                if dphase = phases_t'high then
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
        variable cps_downcount : natural range 0 to 2;
    begin
        if (reset = '1') then
            downcount := 0;
            symbol_mixed_up_valid <= '0';
        elsif rising_edge(clock) then
            symbol_mixed_up_valid <= '0';

            if symbol_mixed_valid = '1' then
                symbol_mixed_up <= symbol_mixed;
                downcount := SPS-1;
                cps_downcount := CPS-1;
                symbol_mixed_up_valid <= '1';

            elsif downcount > 0 then
                symbol_mixed_up <= ((others => '0'), (others => '0'));
                if cps_downcount = 0 then
                    symbol_mixed_up_valid <= '1';
                    downcount := downcount -1;
                    cps_downcount := CPS-1;
                else
                    cps_downcount := cps_downcount -1;
                end if;
            end if;
        end if;
    end process;

    --filter to bandwidth
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
            tx_pilot <= ((others=>'0'),(others=>'0'));
            tx_symbol_valid <= '0';
            pilot := 0;
        elsif rising_edge(clock) then
            tx_symbol_valid <= '0';

            if filtered_sample_valid = '1' then
                tx_symbol.re <= filtered_sample.re + shift_right(atsc_lo_table(pilot).re,2);
                tx_symbol.im <= filtered_sample.im + shift_right(atsc_lo_table(pilot).im,2);

                tx_pilot.re <=  shift_right(atsc_lo_table(pilot).re,1);
                tx_pilot.im <=  shift_right(atsc_lo_table(pilot).im,1);
                tx_symbol_valid <= '1';

                if pilot = atsc_lo_table'high then
                    pilot := 0;
                else
                    pilot := pilot +1;
                end if;
            end if;
        end if;
    end process;

    register_output : process(clock,reset)
    begin
        if (reset = '1' ) then
            registered_symbol <= ((others=> '0'),(others=>'0'));
            registered_symbol_valid <= '0';
        elsif rising_edge(clock) then
            registered_symbol.re <= shift_right(tx_symbol.re,2);
            registered_symbol.im <= shift_right(tx_symbol.im,2);
            registered_symbol_valid <= tx_symbol_valid;
        end if;
    end process;

end architecture;
