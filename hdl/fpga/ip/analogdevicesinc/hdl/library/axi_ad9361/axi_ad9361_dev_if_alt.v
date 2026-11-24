// ***************************************************************************
// ***************************************************************************
// Copyright 2011(c) Analog Devices, Inc.
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//     - Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     - Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     - Neither the name of Analog Devices, Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//     - The use of this software may or may not infringe the patent rights
//       of one or more patent holders.  This license does not release you
//       from the requirement that you obtain separate licenses from these
//       patent holders to use this software.
//     - Use of the software either in source or binary form, must be run
//       on or directly connected to an Analog Devices Inc. component.
//    
// THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED.
//
// IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, INTELLECTUAL PROPERTY
// RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// ***************************************************************************
// ***************************************************************************
// This interface includes both the transmit and receive components -
// They both uses the same clock (sourced from the receiving side).

`timescale 1ns/100ps

module axi_ad9361_dev_if (

  // physical interface (receive)

  rx_clk_in_p,
  rx_clk_in_n,
  rx_frame_in_p,
  rx_frame_in_n,
  rx_data_in_p,
  rx_data_in_n,

  // physical interface (transmit)

  tx_clk_out_p,
  tx_clk_out_n,
  tx_frame_out_p,
  tx_frame_out_n,
  tx_data_out_p,
  tx_data_out_n,

  // ensm control

  enable,
  txnrx,

  // clock (common to both receive and transmit)

  rst,
  clk,
  l_clk,

  // receive data path interface

  adc_valid,
  adc_data,
  adc_status,
  adc_r1_mode,
  adc_ddr_edgesel,

  // transmit data path interface

  dac_valid,
  dac_data,
  dac_r1_mode,

  // tdd interface

  tdd_enable,
  tdd_txnrx,
  tdd_mode,

  // delay interface

  up_clk,
  up_enable,
  up_txnrx,
  up_adc_dld,
  up_adc_dwdata,
  up_adc_drdata,
  up_dac_dld,
  up_dac_dwdata,
  up_dac_drdata,
  delay_clk,
  delay_rst,
  delay_locked);

  // this parameter controls the buffer type based on the target device.

  parameter   DEVICE_TYPE = 0;
  parameter   DAC_IODELAY_ENABLE = 0;
  parameter   IO_DELAY_GROUP = "dev_if_delay_group";

  // physical interface (receive)

  input           rx_clk_in_p;
  input           rx_clk_in_n;
  input           rx_frame_in_p;
  input           rx_frame_in_n;
  input   [ 5:0]  rx_data_in_p;
  input   [ 5:0]  rx_data_in_n;

  // physical interface (transmit)

  output          tx_clk_out_p;
  output          tx_clk_out_n;
  output          tx_frame_out_p;
  output          tx_frame_out_n;
  output  [ 5:0]  tx_data_out_p;
  output  [ 5:0]  tx_data_out_n;

  // ensm control

  output          enable;
  output          txnrx;

  // clock (common to both receive and transmit)

  input           rst;
  input           clk;
  output          l_clk;

  // receive data path interface

  output          adc_valid;
  output  [47:0]  adc_data;
  output          adc_status;
  input           adc_r1_mode;
  input           adc_ddr_edgesel;

  // transmit data path interface

  input           dac_valid;
  input   [47:0]  dac_data;
  input           dac_r1_mode;

  // tdd interface

  input           tdd_enable;
  input           tdd_txnrx;
  input           tdd_mode;

  // delay interface

  input           up_clk;
  input           up_enable;
  input           up_txnrx;
  input   [ 6:0]  up_adc_dld;
  input   [34:0]  up_adc_dwdata;
  output  [34:0]  up_adc_drdata; // = 'h0;
  input   [ 9:0]  up_dac_dld;
  input   [49:0]  up_dac_dwdata;
  output  [49:0]  up_dac_drdata; // = 'h0;
  input           delay_clk;
  input           delay_rst;
  output          delay_locked;

  // internal registers

  reg             rx_r1_mode = 'd0;
  reg             rx_r1_mode_r2 = 'd0;
  reg             tx_r1_mode = 'd0;
  reg     [ 3:0]  rx_frame = 'd0;
  reg     [ 5:0]  rx_data_3 = 'd0;
  reg     [ 5:0]  rx_data_2 = 'd0;
  reg     [ 5:0]  rx_data_1 = 'd0;
  reg     [ 5:0]  rx_data_0 = 'd0;
  reg             rx_error_r2 = 'd0;
  reg             rx_valid_r2 = 'd0;
  reg     [23:0]  rx_data_r2 = 'd0;
  reg             adc_valid = 'd0;
  reg     [47:0]  adc_data = 'd0;
  reg             adc_status = 'd0;
  reg             tx_data_sel = 'd0;
  reg     [47:0]  tx_data = 'd0;
  reg     [ 3:0]  tx_frame = 'd0;
  reg     [ 5:0]  tx_data_0 = 'd0;
  reg     [ 5:0]  tx_data_1 = 'd0;
  reg     [ 5:0]  tx_data_2 = 'd0;
  reg     [ 5:0]  tx_data_3 = 'd0;

  // internal signals

  wire    [ 3:0]  rx_frame_inv_s;
  wire            tx_locked_s;
  wire    [ 3:0]  rx_frame_s;
  wire    [ 5:0]  rx_data_0_s;
  wire    [ 5:0]  rx_data_1_s;
  wire    [ 5:0]  rx_data_2_s;
  wire    [ 5:0]  rx_data_3_s;
  wire            rx_locked_s;
  wire            rx_pll_reset_s;
  wire            tx_pll_reset_s;

  // up_adc_drdata and up_dac_drdata explicitly assigned here
  // the default on the output declaration breaks modelsim...
  assign up_adc_drdata = 'h0;
  assign up_dac_drdata = 'h0;

  // tdd support-

  assign enable = up_enable;
  assign txnrx = up_txnrx;

  // defaults

  assign delay_locked = 1'd1;

  // receive data path interface

  // Each l_clk cycle carries four 6-bit words. rx_frame[0] and rx_data_0 are
  // the first word in time. A sample is four words: I msb, Q msb, I lsb, Q lsb.
  // In 2R2T mode the frame is high for the four words of channel 1 and low for
  // the four words of channel 2, so adc_valid is high every other cycle with
  // both channels in adc_data. In 1R1T (R1) mode the frame is high for the two
  // msb words of each sample, so every cycle holds one of 4'b0011, 4'b0110,
  // 4'b1100 or 4'b1001 and starts one sample. adc_valid is then high every
  // cycle with the one channel in adc_data[23:0]. A sample that starts on word
  // n uses the same word mapping in both modes.

  assign rx_frame_inv_s = ~rx_frame;

  always @(posedge l_clk) begin
    rx_r1_mode <= adc_r1_mode;
    rx_r1_mode_r2 <= rx_r1_mode;
    rx_frame <= rx_frame_s;
    rx_data_3 <= rx_data_3_s;
    rx_data_2 <= rx_data_2_s;
    rx_data_1 <= rx_data_1_s;
    rx_data_0 <= rx_data_0_s;
    if (rx_r1_mode == 1'b1) begin
      if (rx_frame == rx_frame_s) begin
        rx_error_r2 <= 1'b0;
      end else begin
        rx_error_r2 <= 1'b1;
      end
    end else if (rx_frame_inv_s == rx_frame_s) begin
      rx_error_r2 <= 1'b0;
    end else begin
      rx_error_r2 <= 1'b1;
    end
    case ({rx_r1_mode, rx_frame})
      5'b10011: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_1,   rx_data_3};
        rx_data_r2[11: 0] <= {rx_data_0,   rx_data_2};
      end
      5'b10110: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_2,   rx_data_0_s};
        rx_data_r2[11: 0] <= {rx_data_1,   rx_data_3};
      end
      5'b11100: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_3,   rx_data_1_s};
        rx_data_r2[11: 0] <= {rx_data_2,   rx_data_0_s};
      end
      5'b11001: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_0_s, rx_data_2_s};
        rx_data_r2[11: 0] <= {rx_data_3,   rx_data_1_s};
      end
      5'b01111: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_1,   rx_data_3};
        rx_data_r2[11: 0] <= {rx_data_0,   rx_data_2};
      end
      5'b01110: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_2,   rx_data_0_s};
        rx_data_r2[11: 0] <= {rx_data_1,   rx_data_3};
      end
      5'b01100: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_3,   rx_data_1_s};
        rx_data_r2[11: 0] <= {rx_data_2,   rx_data_0_s};
      end
      5'b01000: begin
        rx_valid_r2 <= 1'b1;
        rx_data_r2[23:12] <= {rx_data_0_s, rx_data_2_s};
        rx_data_r2[11: 0] <= {rx_data_3,   rx_data_1_s};
      end
      5'b00000: begin
        rx_valid_r2 <= 1'b0;
        rx_data_r2[23:12] <= {rx_data_1,   rx_data_3};
        rx_data_r2[11: 0] <= {rx_data_0,   rx_data_2};
      end
      5'b00001: begin
        rx_valid_r2 <= 1'b0;
        rx_data_r2[23:12] <= {rx_data_2,   rx_data_0_s};
        rx_data_r2[11: 0] <= {rx_data_1,   rx_data_3};
      end
      5'b00011: begin
        rx_valid_r2 <= 1'b0;
        rx_data_r2[23:12] <= {rx_data_3,   rx_data_1_s};
        rx_data_r2[11: 0] <= {rx_data_2,   rx_data_0_s};
      end
      5'b00111: begin
        rx_valid_r2 <= 1'b0;
        rx_data_r2[23:12] <= {rx_data_0_s, rx_data_2_s};
        rx_data_r2[11: 0] <= {rx_data_3,   rx_data_1_s};
      end
      default: begin
        rx_valid_r2 <= 1'b0;
        rx_data_r2[23:12] <= 12'd0;
        rx_data_r2[11: 0] <= 12'd0;
      end
    endcase
    if (rx_r1_mode_r2 == 1'b1) begin
      adc_valid <= rx_valid_r2;
      adc_data <= {24'd0, rx_data_r2};
    end else if (rx_valid_r2 == 1'b1) begin
      adc_valid <= 1'b0;
      adc_data <= {24'd0, rx_data_r2};
    end else begin
      adc_valid <= 1'b1;
      adc_data <= {rx_data_r2, adc_data[23:0]};
    end
    adc_status <= ~rx_error_r2 & rx_locked_s & tx_locked_s;
  end

  // transmit data path mux

  // In R1 mode the frame is high for the two msb words of every sample, and
  // the core sends one sample per cycle (dac_valid every cycle, DAC rate 0).

  always @(posedge l_clk) begin
    tx_r1_mode <= dac_r1_mode;
    tx_data_sel <= dac_valid;
    tx_data <= dac_data;
    if (tx_r1_mode == 1'b1) begin
      tx_frame <= 4'b0011;
      tx_data_0 <= tx_data[11: 6];
      tx_data_1 <= tx_data[23:18];
      tx_data_2 <= tx_data[ 5: 0];
      tx_data_3 <= tx_data[17:12];
    end else if (tx_data_sel == 1'b1) begin
      tx_frame <= 4'b1111;
      tx_data_0 <= tx_data[11: 6];
      tx_data_1 <= tx_data[23:18];
      tx_data_2 <= tx_data[ 5: 0];
      tx_data_3 <= tx_data[17:12];
    end else begin
      tx_frame <= 4'b0000;
      tx_data_0 <= tx_data[35:30];
      tx_data_1 <= tx_data[47:42];
      tx_data_2 <= tx_data[29:24];
      tx_data_3 <= tx_data[41:36];
    end
  end

  // interface (transmit)

  //pll_reset #(
  //  .SYS_CLOCK_FREQ_HZ( 80000000 ),
  //  .DEVICE_FAMILY( "Cyclone V" ) )
  //i_tx_pll_reset (
  //  .sys_clock(up_clk),
  //  .pll_locked(tx_locked_s),
  //  .pll_locked_out( ),
  //  .pll_reset(tx_pll_reset_s)
  //);

  assign tx_pll_reset_s = rx_pll_reset_s;

  axi_ad9361_alt_lvds_tx i_tx (
    .tx_clk_out_p (tx_clk_out_p),
    .tx_clk_out_n (tx_clk_out_n),
    .tx_frame_out_p (tx_frame_out_p),
    .tx_frame_out_n (tx_frame_out_n),
    .tx_data_out_p (tx_data_out_p),
    .tx_data_out_n (tx_data_out_n),
    .tx_clk (rx_clk_in_p),
    .clk (l_clk),
    .tx_frame (tx_frame),
    .tx_data_0 (tx_data_0),
    .tx_data_1 (tx_data_1),
    .tx_data_2 (tx_data_2),
    .tx_data_3 (tx_data_3),
    .tx_locked (tx_locked_s),
    .tx_pll_reset(tx_pll_reset_s)
  );

  // interface (receive)

  pll_reset #(
    .SYS_CLOCK_FREQ_HZ( 80000000 ),
    .DEVICE_FAMILY( "Cyclone V" ) )
  i_rx_pll_reset (
    .sys_clock(up_clk),
    .pll_locked(rx_locked_s & tx_locked_s), // TX & RX PLLs will be merged anyway
    .pll_locked_out( ),
    .pll_reset(rx_pll_reset_s)
  );

  axi_ad9361_alt_lvds_rx i_rx (
    .rx_clk_in_p (rx_clk_in_p),
    .rx_clk_in_n (rx_clk_in_n),
    .rx_frame_in_p (rx_frame_in_p),
    .rx_frame_in_n (rx_frame_in_n),
    .rx_data_in_p (rx_data_in_p),
    .rx_data_in_n (rx_data_in_n),
    .clk (l_clk),
    .rx_frame (rx_frame_s),
    .rx_data_0 (rx_data_0_s),
    .rx_data_1 (rx_data_1_s),
    .rx_data_2 (rx_data_2_s),
    .rx_data_3 (rx_data_3_s),
    .rx_locked (rx_locked_s),
    .rx_pll_reset(rx_pll_reset_s)
  );

endmodule

// ***************************************************************************
// ***************************************************************************
