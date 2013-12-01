library ieee ;
    use ieee.std_logic_1164.all ;

entity nios_system is
  port (
    clk_clk             :   in  std_logic ;
    reset_reset_n       :   in  std_logic ;
    dac_MISO            :   in  std_logic ;
    dac_MOSI            :   out std_logic ;
    dac_SCLK            :   out std_logic ;
    dac_SS_n            :   out std_logic ;
    spi_MISO            :   in  std_logic ;
    spi_MOSI            :   out std_logic ;
    spi_SCLK            :   out std_logic ;
    spi_SS_n            :   out std_logic ;
    uart_rxd            :   in  std_logic ;
    uart_txd            :   out std_logic ;
    oc_i2c_scl_pad_i    :   in  std_logic ;
    oc_i2c_scl_pad_o    :   out std_logic ;
    oc_i2c_scl_padoen_o :   out std_logic ;
    oc_i2c_sda_pad_i    :   in  std_logic ;
    oc_i2c_sda_pad_o    :   out std_logic ;
    oc_i2c_sda_padoen_o :   out std_logic ;
    oc_i2c_arst_i       :   in  std_logic ;
    gpio_export         :   out std_logic_vector(31 downto 0)
  ) ;
end entity ;

architecture sim of nios_system is

begin

    dac_MOSI <= '0' ;
    dac_SCLK <= '0' ;
    dac_SS_n <= '1' ;

    spi_MOSI <= '0' ;
    spi_SCLK <= '0' ;
    spi_SS_n <= '1' ;

    uart_txd <= '1' ;

    oc_i2c_scl_pad_o <= '0' ;
    oc_i2c_scl_padoen_o <= '1' ;
    oc_i2c_sda_pad_o <= '0' ;
    oc_i2c_sda_padoen_o <= '1' ;

    gpio_export <= x"0000_0057" ;

end architecture ;

