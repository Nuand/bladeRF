-- The only purpose for this wrapper is to set sane defaults for the
-- DCFIFO generics. These defaults may be overridden as needed.

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.math_real.all;

library altera_mf;
    use altera_mf.altera_mf_components.all;

entity rx_meta_fifo is
    generic (
        ADD_RAM_OUTPUT_REGISTER : string  := "OFF";
        ADD_USEDW_MSB_BIT       : string  := "OFF";
        CLOCKS_ARE_SYNCHRONIZED : string  := "FALSE";
        DELAY_RDUSEDW           : natural := 1;
        DELAY_WRUSEDW           : natural := 1;
        INTENDED_DEVICE_FAMILY  : string  := "Cyclone V"; -- "unused";
        --ENABLE_ECC              : string  := "FALSE";
        LPM_NUMWORDS            : natural := 32;
        LPM_SHOWAHEAD           : string  := "ON";
        LPM_WIDTH               : natural := 128;
        LPM_WIDTH_R             : natural := 32;
        --LPM_WIDTHU              : natural := 5;
        --LPM_WIDTHU_R            : natural := 7;
        OVERFLOW_CHECKING       : string  := "ON";
        RDSYNC_DELAYPIPE        : natural := 5;
        READ_ACLR_SYNCH         : string  := "ON";
        UNDERFLOW_CHECKING      : string  := "ON";
        USE_EAB                 : string  := "ON";
        WRITE_ACLR_SYNCH        : string  := "ON";
        WRSYNC_DELAYPIPE        : natural := 5;
        LPM_HINT                : string  := "UNUSED";
        LPM_TYPE                : string  := "dcfifo_mixed_widths"
    );
    port (
        aclr       : in  std_logic := '0';
        data       : in  std_logic_vector(LPM_WIDTH-1 downto 0);
        rdclk      : in  std_logic;
        rdreq      : in  std_logic;
        wrclk      : in  std_logic;
        wrreq      : in  std_logic;
        q          : out std_logic_vector(LPM_WIDTH_R-1 downto 0);
        rdempty    : out std_logic;
        rdfull     : out std_logic;
        rdusedw    : out std_logic_vector(integer(ceil(log2(real(LPM_NUMWORDS*LPM_WIDTH)/real(LPM_WIDTH_R))))-1 downto 0);
        wrempty    : out std_logic;
        wrfull     : out std_logic;
        wrusedw    : out std_logic_vector(integer(ceil(log2(real(LPM_NUMWORDS))))-1 downto 0)
        --eccstatus  : out std_logic_vector(1 downto 0)
    );
end entity;

architecture arch of rx_meta_fifo is

begin

    U_dcfifo_mixed_widths : dcfifo_mixed_widths
        generic map (
            ADD_RAM_OUTPUT_REGISTER => ADD_RAM_OUTPUT_REGISTER,
            ADD_USEDW_MSB_BIT       => ADD_USEDW_MSB_BIT,
            CLOCKS_ARE_SYNCHRONIZED => CLOCKS_ARE_SYNCHRONIZED,
            DELAY_RDUSEDW           => DELAY_RDUSEDW,
            DELAY_WRUSEDW           => DELAY_WRUSEDW,
            INTENDED_DEVICE_FAMILY  => INTENDED_DEVICE_FAMILY,
            --ENABLE_ECC              => ENABLE_ECC,
            LPM_NUMWORDS            => LPM_NUMWORDS,
            LPM_SHOWAHEAD           => LPM_SHOWAHEAD,
            LPM_WIDTH               => LPM_WIDTH,
            LPM_WIDTH_R             => LPM_WIDTH_R,
            LPM_WIDTHU              => wrusedw'length,
            LPM_WIDTHU_R            => rdusedw'length,
            OVERFLOW_CHECKING       => OVERFLOW_CHECKING,
            RDSYNC_DELAYPIPE        => RDSYNC_DELAYPIPE,
            READ_ACLR_SYNCH         => READ_ACLR_SYNCH,
            UNDERFLOW_CHECKING      => UNDERFLOW_CHECKING,
            USE_EAB                 => USE_EAB,
            WRITE_ACLR_SYNCH        => WRITE_ACLR_SYNCH,
            WRSYNC_DELAYPIPE        => WRSYNC_DELAYPIPE,
            LPM_HINT                => LPM_HINT,
            LPM_TYPE                => LPM_TYPE
        )
        port map (
            aclr      => aclr,
            data      => data,
            rdclk     => rdclk,
            rdreq     => rdreq,
            wrclk     => wrclk,
            wrreq     => wrreq,
            q         => q,
            rdempty   => rdempty,
            rdfull    => rdfull,
            rdusedw   => rdusedw,
            wrempty   => wrempty,
            wrfull    => wrfull,
            wrusedw   => wrusedw
            --eccstatus => eccstatus
        );

end architecture;
