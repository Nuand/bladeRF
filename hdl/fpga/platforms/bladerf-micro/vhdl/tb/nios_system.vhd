-- Copyright (c) 2013-2017 Nuand LLC
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

library ieee ;
    use ieee.std_logic_1164.all ;

entity nios_system is
    port (
        ad9361_adc_i0_enable            : out std_logic;                                        -- enable
        ad9361_adc_i0_valid             : out std_logic;                                        -- valid
        ad9361_adc_i0_data              : out std_logic_vector(15 downto 0);                    -- data
        ad9361_adc_i1_enable            : out std_logic;                                        -- enable
        ad9361_adc_i1_valid             : out std_logic;                                        -- valid
        ad9361_adc_i1_data              : out std_logic_vector(15 downto 0);                    -- data
        ad9361_adc_overflow_ovf         : in  std_logic                     := 'X';             -- ovf
        ad9361_adc_q0_enable            : out std_logic;                                        -- enable
        ad9361_adc_q0_valid             : out std_logic;                                        -- valid
        ad9361_adc_q0_data              : out std_logic_vector(15 downto 0);                    -- data
        ad9361_adc_q1_enable            : out std_logic;                                        -- enable
        ad9361_adc_q1_valid             : out std_logic;                                        -- valid
        ad9361_adc_q1_data              : out std_logic_vector(15 downto 0);                    -- data
        ad9361_adc_underflow_unf        : in  std_logic                     := 'X';             -- unf
        ad9361_dac_i0_enable            : out std_logic;                                        -- enable
        ad9361_dac_i0_valid             : out std_logic;                                        -- valid
        ad9361_dac_i0_data              : in  std_logic_vector(15 downto 0) := (others => 'X'); -- data
        ad9361_dac_i1_enable            : out std_logic;                                        -- enable
        ad9361_dac_i1_valid             : out std_logic;                                        -- valid
        ad9361_dac_i1_data              : in  std_logic_vector(15 downto 0) := (others => 'X'); -- data
        ad9361_dac_overflow_ovf         : in  std_logic                     := 'X';             -- ovf
        ad9361_dac_q0_enable            : out std_logic;                                        -- enable
        ad9361_dac_q0_valid             : out std_logic;                                        -- valid
        ad9361_dac_q0_data              : in  std_logic_vector(15 downto 0) := (others => 'X'); -- data
        ad9361_dac_q1_enable            : out std_logic;                                        -- enable
        ad9361_dac_q1_valid             : out std_logic;                                        -- valid
        ad9361_dac_q1_data              : in  std_logic_vector(15 downto 0) := (others => 'X'); -- data
        ad9361_dac_sync_in_sync         : in  std_logic                     := 'X';             -- sync
        ad9361_dac_sync_out_sync        : out std_logic;                                        -- sync
        ad9361_dac_underflow_unf        : in  std_logic                     := 'X';             -- unf
        ad9361_data_clock_clk           : out std_logic;                                        -- clk
        ad9361_data_reset_reset         : out std_logic;                                        -- reset
        ad9361_device_if_rx_clk_in_p    : in  std_logic                     := 'X';             -- rx_clk_in_p
        ad9361_device_if_rx_clk_in_n    : in  std_logic                     := 'X';             -- rx_clk_in_n
        ad9361_device_if_rx_frame_in_p  : in  std_logic                     := 'X';             -- rx_frame_in_p
        ad9361_device_if_rx_frame_in_n  : in  std_logic                     := 'X';             -- rx_frame_in_n
        ad9361_device_if_rx_data_in_p   : in  std_logic_vector(5 downto 0)  := (others => 'X'); -- rx_data_in_p
        ad9361_device_if_rx_data_in_n   : in  std_logic_vector(5 downto 0)  := (others => 'X'); -- rx_data_in_n
        ad9361_device_if_tx_clk_out_p   : out std_logic;                                        -- tx_clk_out_p
        ad9361_device_if_tx_clk_out_n   : out std_logic;                                        -- tx_clk_out_n
        ad9361_device_if_tx_frame_out_p : out std_logic;                                        -- tx_frame_out_p
        ad9361_device_if_tx_frame_out_n : out std_logic;                                        -- tx_frame_out_n
        ad9361_device_if_tx_data_out_p  : out std_logic_vector(5 downto 0);                     -- tx_data_out_p
        ad9361_device_if_tx_data_out_n  : out std_logic_vector(5 downto 0);                     -- tx_data_out_n
        clk_clk                         : in  std_logic                     := 'X';             -- clk
        command_serial_in               : in  std_logic                     := 'X';             -- serial_in
        command_serial_out              : out std_logic;                                        -- serial_out
        dac_MISO                        : in  std_logic                     := 'X';             -- MISO
        dac_MOSI                        : out std_logic;                                        -- MOSI
        dac_SCLK                        : out std_logic;                                        -- SCLK
        dac_SS_n                        : out std_logic_vector(1 downto 0);                     -- SS_n
        gpio_in_port                    : in  std_logic_vector(31 downto 0);                    -- in_port
        gpio_out_port                   : out std_logic_vector(31 downto 0);                    -- out_port
        gpio_rffe_0_in_port             : in  std_logic_vector(31 downto 0) := (others => 'X'); -- in_port
        gpio_rffe_0_out_port            : out std_logic_vector(31 downto 0);                    -- out_port
        oc_i2c_scl_pad_o                : out std_logic;                                        -- scl_pad_o
        oc_i2c_scl_padoen_o             : out std_logic;                                        -- scl_padoen_o
        oc_i2c_sda_pad_i                : in  std_logic                     := 'X';             -- sda_pad_i
        oc_i2c_sda_pad_o                : out std_logic;                                        -- sda_pad_o
        oc_i2c_sda_padoen_o             : out std_logic;                                        -- sda_padoen_o
        oc_i2c_arst_i                   : in  std_logic                     := 'X';             -- arst_i
        oc_i2c_scl_pad_i                : in  std_logic                     := 'X';             -- scl_pad_i
        reset_reset_n                   : in  std_logic                     := 'X';             -- reset_n
        rx_tamer_ts_sync_in             : in  std_logic                     := 'X';             -- ts_sync_in
        rx_tamer_ts_sync_out            : out std_logic;                                        -- ts_sync_out
        rx_tamer_ts_pps                 : in  std_logic                     := 'X';             -- ts_pps
        rx_tamer_ts_clock               : in  std_logic                     := 'X';             -- ts_clock
        rx_tamer_ts_reset               : in  std_logic                     := 'X';             -- ts_reset
        rx_tamer_ts_time                : out std_logic_vector(63 downto 0);                    -- ts_time
        rx_trigger_ctl_in_port          : in  std_logic_vector(7 downto 0)  := (others => 'X'); -- in_port
        rx_trigger_ctl_out_port         : out std_logic_vector(7 downto 0);                     -- out_port
        spi_MISO                        : in  std_logic                     := 'X';             -- MISO
        spi_MOSI                        : out std_logic;                                        -- MOSI
        spi_SCLK                        : out std_logic;                                        -- SCLK
        spi_SS_n                        : out std_logic;                                        -- SS_n
        tx_tamer_ts_sync_in             : in  std_logic                     := 'X';             -- ts_sync_in
        tx_tamer_ts_sync_out            : out std_logic;                                        -- ts_sync_out
        tx_tamer_ts_pps                 : in  std_logic                     := 'X';             -- ts_pps
        tx_tamer_ts_clock               : in  std_logic                     := 'X';             -- ts_clock
        tx_tamer_ts_reset               : in  std_logic                     := 'X';             -- ts_reset
        tx_tamer_ts_time                : out std_logic_vector(63 downto 0);                    -- ts_time
        tx_trigger_ctl_in_port          : in  std_logic_vector(7 downto 0)  := (others => 'X'); -- in_port
        tx_trigger_ctl_out_port         : out std_logic_vector(7 downto 0);                     -- out_port
        xb_gpio_in_port                 : in  std_logic_vector(31 downto 0) := (others => 'X'); -- in_port
        xb_gpio_out_port                : out std_logic_vector(31 downto 0);                    -- out_port
        xb_gpio_dir_export              : out std_logic_vector(31 downto 0)                     -- export
    );
end entity ;

architecture sim of nios_system is

begin

    command_serial_out <= '1' ;

    dac_MOSI <= '0' ;
    dac_SCLK <= '0' ;
    dac_SS_n <= (others =>'1') ;

    gpio_out_port <= x"0001_0157" after 1 us ;

    oc_i2c_scl_pad_o <= '0' ;
    oc_i2c_scl_padoen_o <= '1' ;
    oc_i2c_sda_pad_o <= '0' ;
    oc_i2c_sda_padoen_o <= '1' ;

    rx_tamer_ts_sync_out <= '0' ;
    rx_tamer_ts_time <= (others =>'0') ;

    spi_MOSI <= '0' ;
    spi_SCLK <= '0' ;
    spi_SS_n <= '1' ;

    tx_tamer_ts_sync_out <= '0' ;
    tx_tamer_ts_time <= (others =>'0') ;

    xb_gpio_out_port <= (others =>'0') ;
    xb_gpio_dir_export <= (others =>'0') ;

end architecture ;

