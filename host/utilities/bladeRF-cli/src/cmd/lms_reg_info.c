/*
 * @file lms_reg_info.c
 *
 * @brief Print additional LMS register information
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>

/* Get the value of val[u:l] (u="upper", l="lower") */
#define BITS(val, u, l) ((val >> l) & ((1 << (u - l + 1)) - 1))

/* Print the value of val[u:l], with a description */
#define PRINT_BITS(desc, u, l, val) do { \
        const char bitstr[] = "[" #u ":" #l "]"; \
        unsigned int bitsval = BITS(val, u, l); \
        printf("    %-10s%3d (0x%02x)      %-32s\n", \
               bitstr, bitsval, bitsval, desc); \
    } while (0)

void lms_reg_info(unsigned int address, unsigned int value)
{
    switch (address) {
        case 0x00:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_REGVAL[5:0]", 5, 0, value);
            break;

        case 0x01:
            PRINT_BITS("RCCAL_LPFCAL[2:0]", 7, 5, value);
            PRINT_BITS("DC_LOCK[2:0]", 4, 2, value);
            PRINT_BITS("DC_CLBR_DONE", 1, 1, value);
            PRINT_BITS("DC_UD", 0, 0, value);
            break;

        case 0x02:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_CNTVAL[5:0]", 5, 0, value);
            break;

        case 0x03:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_START_CLBR", 5, 5, value);
            PRINT_BITS("DC_LOAD", 4, 4, value);
            PRINT_BITS("DC_SRESET", 3, 3, value);
            PRINT_BITS("DC_ADDR[2:0]", 2, 0, value);
            break;

        case 0x04:
            PRINT_BITS("VER[3:0]", 7, 4, value);
            PRINT_BITS("REV[3:0]", 3, 0, value);
            break;

        case 0x05:
            PRINT_BITS("DECODE", 7, 7, value);
            PRINT_BITS("Not used", 6, 6, value);
            PRINT_BITS("SRESET", 5, 5, value);
            PRINT_BITS("EN", 4, 4, value);
            PRINT_BITS("STXEN", 3, 3, value);
            PRINT_BITS("SRXEN", 2, 2, value);
            PRINT_BITS("TFWMODE", 1, 1, value);
            PRINT_BITS("Not used", 0, 0, value);
            break;

        case 0x06:
            PRINT_BITS("Not used", 7, 4, value);
            PRINT_BITS("CLKSEL_LPFCAL", 3, 3, value);
            PRINT_BITS("PD_CLKLPFCAL", 2, 2, value);
            PRINT_BITS("ENF_EN_CAL_LPFCAL", 1, 1, value);
            PRINT_BITS("RST_CAL_LPFCAL", 0, 0, value);
            break;

        case 0x07:
            PRINT_BITS("EN_CAL_LPFCAL", 7, 7, value);
            PRINT_BITS("FORCE_CODE_CAL_LPFCAL[2:0]", 6, 4, value);
            PRINT_BITS("BWC_LPFCAL[3:0]", 3, 0, value);
            break;

        case 0x08:
            PRINT_BITS("Reserved", 7, 7, value);
            PRINT_BITS("LBEN_LPFIN", 6, 6, value);
            PRINT_BITS("LBEN_VGA2IN", 5, 5, value);
            PRINT_BITS("LBEN_OPIN", 4, 4, value);
            PRINT_BITS("LBRFEN[3:0]", 3, 0, value);
            break;

        case 0x09:
            PRINT_BITS("RXOUTSW", 7, 7, value);
            PRINT_BITS("CLK_EN[6:0]", 6, 0, value);
            PRINT_BITS("PLLCKLOUT CLK_EN", 6, 6, value);
            PRINT_BITS("LPF_CAL CLK_EN", 5, 5, value);
            PRINT_BITS("RX VGA2 CLK_EN", 4, 4, value);
            PRINT_BITS("RX LPF CLK_EN", 3, 3, value);
            PRINT_BITS("RX DSM CLK_EN", 2, 2, value);
            PRINT_BITS("TX LPF CLK_EN", 1, 1, value);
            PRINT_BITS("TX DSM CLK_EN", 0, 0, value);
            break;

        case 0x0a:
            PRINT_BITS("Not used", 7, 2, value);
            PRINT_BITS("FDDTDD", 1, 1, value);
            PRINT_BITS("TDDMOD", 0, 0, value);
            break;

        case 0x0b:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("PDXCOBUF", 4, 4, value);
            PRINT_BITS("SLFBXCOBUF", 3, 3, value);
            PRINT_BITS("BYPXCOBUF", 2, 2, value);
            PRINT_BITS("PD[1:0]", 1, 0, value);
            PRINT_BITS("PD_DCOREF_LPFCAL", 1, 1, value);
            PRINT_BITS("PD RF loopback switch", 0, 0, value);
            break;

        case 0x0e:
            PRINT_BITS("SPARE0[7:0]", 5, 0, value);
            break;

        case 0x0f:
            PRINT_BITS("SPARE0[7:0]", 7, 0, value);
            break;

        case 0x10:
            PRINT_BITS("TX NINT[8:1]", 7, 0, value);
            break;

        case 0x11:
            PRINT_BITS("TX NINT[0]", 7, 7, value);
            PRINT_BITS("TX NFRAC[22:16]", 6, 0, value);
            break;

        case 0x12:
            PRINT_BITS("TX NFRAC[15:8]", 7, 0, value);
            break;

        case 0x13:
            PRINT_BITS("TX NFRAC[7:0]", 7, 0, value);
            break;

        case 0x14:
            PRINT_BITS("TX DITHEN", 7, 7, value);
            PRINT_BITS("TX DITHN[2:0]", 6, 4, value);
            PRINT_BITS("TX EN", 3, 3, value);
            PRINT_BITS("TX AUTOBYP", 2, 2, value);
            PRINT_BITS("TX DECODE", 1, 1, value);
            PRINT_BITS("Reserved", 0, 0, value);
            break;

        case 0x15:
            PRINT_BITS("TX FREQSEL[5:0]", 7, 2, value);
            PRINT_BITS("TX SELOUT[1:0]", 1, 0, value);
            break;

        case 0x16:
            PRINT_BITS("TX END_PFD_UP", 7, 7, value);
            PRINT_BITS("TX OEN_TSTD_SX", 6, 6, value);
            PRINT_BITS("TX PASSEN_TSTOD_SD", 5, 5, value);
            PRINT_BITS("TX ICHP[4:0]", 4, 0, value);
            break;

        case 0x17:
            PRINT_BITS("TX BYPVCOREG", 7, 7, value);
            PRINT_BITS("TX PDVCOREG", 6, 6, value);
            PRINT_BITS("TX FSTVCOBG", 5, 5, value);
            PRINT_BITS("TX OFFUP", 4, 0, value);
            break;

        case 0x18:
            PRINT_BITS("TX VOVCOREG[3:1]", 7, 5, value);
            PRINT_BITS("TX OFFDOWN[4:0]", 4, 0, value);
            break;

        case 0x19:
            PRINT_BITS("TX VOVCOREG[0]", 7, 7, value);
            PRINT_BITS("Not used", 6, 6, value);
            PRINT_BITS("TX VCOCAP[5:0]", 5, 0, value);
            break;

        case 0x1a:
            PRINT_BITS("TX VTUNE_H", 7, 7, value);
            PRINT_BITS("TX VTUNE_L", 6, 6, value);
            PRINT_BITS("Reserved", 5, 0, value);
            break;

        case 0x1b:
            PRINT_BITS("Reserved", 7, 4, value);
            PRINT_BITS("PD_VCOCOMP_SX", 3, 3, value);
            PRINT_BITS("Reserved", 2, 0, value);
            break;

        case 0x1c:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x1d:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x1e:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x1f:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x20:
            PRINT_BITS("RX NINT[8:1]", 7, 0, value);
            break;

        case 0x21:
            PRINT_BITS("RX NINT[0]", 7, 7, value);
            PRINT_BITS("RX NFRAC[22:16]", 6, 0, value);
            break;

        case 0x22:
            PRINT_BITS("RX NFRAC[15:8]", 7, 0, value);
            break;

        case 0x23:
            PRINT_BITS("RX NFRAC[7:0]", 7, 0, value);
            break;

        case 0x24:
            PRINT_BITS("RX DITHEN", 7, 7, value);
            PRINT_BITS("RX DITHN[2:0]", 6, 4, value);
            PRINT_BITS("RX EN", 3, 3, value);
            PRINT_BITS("RX AUTOBYP", 2, 2, value);
            PRINT_BITS("RX DECODE", 1, 1, value);
            PRINT_BITS("Reserved", 0, 0, value);
            break;

        case 0x25:
            PRINT_BITS("RX FREQSEL[5:0]", 7, 2, value);
            PRINT_BITS("RX SELOUT[1:0]", 1, 0, value);
            break;

        case 0x26:
            PRINT_BITS("RX END_PFD_UP", 7, 7, value);
            PRINT_BITS("RX OEN_TSTD_SX", 6, 6, value);
            PRINT_BITS("RX PASSEN_TSTOD_SD", 5, 5, value);
            PRINT_BITS("RX ICHP[4:0]", 4, 0, value);
            break;

        case 0x27:
            PRINT_BITS("RX BYPVCOREG", 7, 7, value);
            PRINT_BITS("RX PDVCOREG", 6, 6, value);
            PRINT_BITS("RX FSTVCOBG", 5, 5, value);
            PRINT_BITS("RX OFFUP", 4, 0, value);
            break;

        case 0x28:
            PRINT_BITS("RX VOVCOREG[3:1]", 7, 5, value);
            PRINT_BITS("RX OFFDOWN[4:0]", 4, 0, value);
            break;

        case 0x29:
            PRINT_BITS("RX VOVCOREG[0]", 7, 7, value);
            PRINT_BITS("Not used", 6, 6, value);
            PRINT_BITS("RX VCOCAP[5:0]", 5, 0, value);
            break;

        case 0x2a:
            PRINT_BITS("RX VTUNE_H", 7, 7, value);
            PRINT_BITS("RX VTUNE_L", 6, 6, value);
            PRINT_BITS("Reserved", 5, 0, value);
            break;

        case 0x2b:
            PRINT_BITS("Reserved", 7, 4, value);
            PRINT_BITS("RX PD_VCOCOMP_SX", 3, 3, value);
            PRINT_BITS("RX Reserved", 2, 0, value);
            break;

        case 0x2c:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x2d:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x2e:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x2f:
            PRINT_BITS("Reserved", 7, 0, value);
            break;

        case 0x30:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_REGVAL[5:0]", 5, 0, value);
            break;

        case 0x31:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("DC_LOCK[2:0]", 4, 2, value);
            PRINT_BITS("DC_CLBR_DONE", 1, 1, value);
            PRINT_BITS("DC_UD", 0, 0, value);
            break;

        case 0x32:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_CNTVAL[5:0]", 5, 0, value);
            break;

        case 0x33:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_START_CLBR", 5, 5, value);
            PRINT_BITS("DC_LOAD", 4, 4, value);
            PRINT_BITS("DC_SRESET", 3, 3, value);
            PRINT_BITS("DC_ADDR[2:0]", 2, 0, value);
            break;

        case 0x34:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("BWC_LPF[3:0]", 5, 2, value);
            PRINT_BITS("EN", 1, 1, value);
            PRINT_BITS("DECODE", 0, 0, value);
            break;

        case 0x35:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("BYP_EN_LPF", 6, 6, value);
            PRINT_BITS("DCO_DACCAL[5:0]", 5, 0, value);
            break;

        case 0x36:
            PRINT_BITS("TX_DACBUF_PD", 7, 7, value);
            PRINT_BITS("RCCAL_LPF[2:0]", 6, 4, value);
            PRINT_BITS("PD_DCOCMP_LPF", 3, 3, value);
            PRINT_BITS("PD_DCODAC_LPF", 2, 2, value);
            PRINT_BITS("PD_DCOREF_LPF", 1, 1, value);
            PRINT_BITS("PD_FIL_LPF", 0, 0, value);
            break;

        case 0x3e:
            PRINT_BITS("SPARE0[7:0]", 7, 0, value);
            break;

        case 0x3f:
            PRINT_BITS("PD_DCOCMP_LPF", 7, 7, value);
            PRINT_BITS("SPARE1[6:0]", 6, 0, value);
            break;

        case 0x40:
            PRINT_BITS("Not used", 7, 2, value);
            PRINT_BITS("EN", 1, 1, value);
            PRINT_BITS("DECODE", 0, 0, value);
            break;

        case 0x41:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("VGA1GAIN[4:0]", 4, 0, value);
            break;

        case 0x42:
            PRINT_BITS("VGA1DC_I[7:0]", 7, 0, value);
            break;

        case 0x43:
            PRINT_BITS("VGA1DC_Q[7:0]", 7, 0, value);
            break;

        case 0x44:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("PA_EN[2:0]", 4, 2, value);
            PRINT_BITS("PD_DRVAUX[2:1]", 1, 1, value);
            PRINT_BITS("PD_PKDET[0]", 0, 0, value);
            break;

        case 0x45:
            PRINT_BITS("VGA2GAIN[4:0]", 7, 3, value);
            PRINT_BITS("ENVD[2:0],", 2, 0, value);
            break;

        case 0x46:
            PRINT_BITS("PKDBW[3:0]", 7, 4, value);
            PRINT_BITS("LOOPBBEN[1:0]", 3, 2, value);
            PRINT_BITS("FST_PKDET", 1, 1, value);
            PRINT_BITS("FST_TXHFBIAS", 0, 0, value);
            break;

        case 0x47:
            PRINT_BITS("ICT_TXLOBUF[3:0]", 7, 4, value);
            PRINT_BITS("VBCAS_TXDRV[3:0]", 3, 0, value);
            break;

        case 0x48:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("ICT_TXMIX[4:0]", 4, 0, value);
            break;

        case 0x49:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("ICT_TXDRV[4:0]", 4, 0, value);
            break;

        case 0x4a:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("PW_VGA1_I", 4, 4, value);
            PRINT_BITS("PW_VGA1_Q", 3, 3, value);
            PRINT_BITS("PD_TXDRV", 2, 2, value);
            PRINT_BITS("PD_TXLOBUF", 1, 1, value);
            PRINT_BITS("PD_TXMIX", 0, 0, value);
            break;

        case 0x4b:
            PRINT_BITS("VGA1GAINT[7:0]", 7, 0, value);
            break;

        case 0x4c:
            PRINT_BITS("G_TXVGA2[8:1]", 7, 0, value);
            break;

        case 0x4d:
            PRINT_BITS("G_TXVGA2[0]", 7, 7, value);
            PRINT_BITS("Not used", 6, 0, value);
            break;

        case 0x4e:
            PRINT_BITS("PD_PKDET", 7, 7, value);
            PRINT_BITS("SPARE0[6:0]", 6, 0, value);
            break;

        case 0x4f:
            PRINT_BITS("SPARE1[7:0]", 7, 0, value);

            break;
        case 0x50:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_REGVAL[5:0]", 5, 0, value);
            break;

        case 0x51:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("DC_LOCK[2:0]", 4, 2, value);
            PRINT_BITS("DC_CLBR_DONE", 1, 1, value);
            PRINT_BITS("DC_UD", 0, 0, value);
            break;

        case 0x52:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_CNTVAL[5:0]", 5, 0, value);
            break;

        case 0x53:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_START_CLBR", 5, 5, value);
            PRINT_BITS("DC_LOAD", 4, 4, value);
            PRINT_BITS("DC_SRESET", 3, 3, value);
            PRINT_BITS("DC_ADDR[3:0]", 2, 0, value);
            break;

        case 0x54:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("BWC_LPF[3:0]", 5, 2, value);
            PRINT_BITS("EN_LPF", 1, 1, value);
            PRINT_BITS("DECODE", 0, 0, value);
            break;

        case 0x55:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("BYP_EN_LPF", 6, 6, value);
            PRINT_BITS("DCO_DACCAL[5:0]", 5, 0, value);
            break;

        case 0x56:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("RCCAL_LPF[2:0]", 6, 4, value);
            PRINT_BITS("PD_DCOCMP_LPF", 3, 3, value);
            PRINT_BITS("PD_DCODAC_LPF", 2, 2, value);
            PRINT_BITS("PD_DCOREF_LPF", 1, 1, value);
            PRINT_BITS("PD_FIL_LPF", 0, 0, value);
            break;

        case 0x57:
            PRINT_BITS("EN_ADC_DAC", 7, 7, value);
            PRINT_BITS("DECODE", 6, 6, value);
            PRINT_BITS("TX_CTRL1[6:4]", 5, 3, value);
            PRINT_BITS("TX_CTRL1[3]", 2, 2, value);
            PRINT_BITS("TX_CTRL1[1:0]", 1, 0, value);
            break;

        case 0x58:
            PRINT_BITS("RX_CTRL1[7:6]", 7, 6, value);
            PRINT_BITS("RX_CTRL1[5:4]", 5, 4, value);
            PRINT_BITS("RX_CTRL1[3:0]", 3, 0, value);
            break;

        case 0x59:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("RX_CTRL2[7:6] Reference Gain Adjust", 6, 5, value);
            PRINT_BITS("RX_CTRL2[5:4] Common Mode Adjust", 4, 3, value);
            PRINT_BITS("RX_CTRL2[3:2] Reference Buffer Boost", 2, 1, value);
            PRINT_BITS("RX_CTRL2[0] ADC Input Buffer Disable", 0, 0, value);
            break;

        case 0x5a:
            PRINT_BITS("MISC_CTRL[9] RX Fsync Polarity", 7, 7, value);
            PRINT_BITS("MISC_CTRL[8] RX Interleave Mode", 6, 6, value);
            PRINT_BITS("MISC_CTRL[7] DAC CLK Edge polarity", 5, 5, value);
            PRINT_BITS("MISC_CTRL[6] TX Fsync Polarity", 4, 4, value);
            PRINT_BITS("MISC_CTRL[5] TX Interleave Mode", 3, 3, value);
            PRINT_BITS("RX_CTRL3[7] ADC Sampling Phase", 2, 2, value);
            PRINT_BITS("RX_CTRL3[1:0] Clock Non-overlap Adjust", 1, 0, value);
            break;

        case 0x5b:
            PRINT_BITS("RX_CTRL4[7:6] ADC bias resistor adjust", 7, 6, value);
            PRINT_BITS("RX_CTRL4[5:4] Main bias DOWN", 5, 4, value);
            PRINT_BITS("RX_CTRL4[3:2] ADC Amp1 stage 1 bias UP", 3, 2, value);
            PRINT_BITS("RX_CTRL4[1:0] ADC Amp2 stage 1 bias UP", 1, 0, value);
            break;

        case 0x5c:
            PRINT_BITS("RX_CTRL5[7:6] ADC Amp1 stage 2 bias UP", 7, 6, value);
            PRINT_BITS("RX_CTRL5[5:4] ADC Amp2-4 stage 2 bias UP", 5, 4, value);
            PRINT_BITS("RX_CTRL5[3:2] Quantiazer bus UP", 3, 2, value);
            PRINT_BITS("RX_CTRL5[1:0] Input Buffer bias UP", 1, 0, value);
            break;

        case 0x5d:
            PRINT_BITS("REF_CTRL0[7:4] Bandgap Temperature Coeff Control ",
                       7, 4, value);
            PRINT_BITS("REF_CTRL0[3:0] Bandgap Gain Control", 3, 0, value);
            break;

        case 0x5e:
            PRINT_BITS("REF_CTRL1[7:6] Reference Amps bias adjust",
                       7, 6, value);
            PRINT_BITS("REF_CTRL1[5:4] Reference Amps bias UP", 5, 4, value);
            PRINT_BITS("REF_CTRL1[3:0] Reference Amps bias DOWN", 3, 0, value);
            break;

        case 0x5f:
            PRINT_BITS("PD_DCOCMP_LP", 7, 7, value);
            PRINT_BITS("SPARE00[6:5]", 6, 5, value);
            PRINT_BITS("MISC_CTRL[4] Enable DAC", 4, 4, value);
            PRINT_BITS("MISC_CTRL[3] Enable ADC1", 3, 3, value);
            PRINT_BITS("MISC_CTRL[2] Enable ADC2", 2, 2, value);
            PRINT_BITS("MISC_CTRL[1] Enable ADC reference", 1, 1, value);
            PRINT_BITS("MISC_CTRL[0] Enable master reference", 0, 0, value);
            break;

        case 0x60:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_REGVAL[5:0]", 5, 0, value);
            break;

        case 0x61:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("DC_LOCK[2:0]", 4, 2, value);
            PRINT_BITS("DC_CLBR_DONE", 1, 1, value);
            PRINT_BITS("DC_UD", 0, 0, value);
            break;

        case 0x62:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_CNTVAL[5:0]", 5, 0, value);
            break;

        case 0x63:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("DC_START_CLBR", 5, 5, value);
            PRINT_BITS("DC_LOAD", 4, 4, value);
            PRINT_BITS("DC_SRESET", 3, 3, value);
            PRINT_BITS("DC_ADDR[3:0]", 2, 0, value);
            break;

        case 0x64:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("VCM[3:0]", 5, 2, value);
            PRINT_BITS("EN_RXVGA2", 1, 1, value);
            PRINT_BITS("DECODE", 0, 0, value);
            break;

        case 0x65:
            PRINT_BITS("Not used", 7, 5, value);
            PRINT_BITS("VGA2GAIN[4:0]", 4, 0, value);
            break;

        case 0x66:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("PD[9] DC current regulator", 5, 5, value);
            PRINT_BITS("PD[8] DC calibration DAC for VGA2B", 4, 4, value);
            PRINT_BITS("PD[7] DC calibration comp for VGA2B", 3, 3, value);
            PRINT_BITS("PD[6] DC calibration DAC for VGA2A", 2, 2, value);
            PRINT_BITS("PD[5] DC calibration comp for VGA2A", 1, 1, value);
            PRINT_BITS("PD[4] Band gap", 0, 0, value);
            break;

        case 0x67:
            PRINT_BITS("Not used", 7, 4, value);
            PRINT_BITS("PD[3] Output buffer in both RXVGAs", 3, 3, value);
            PRINT_BITS("PD[2] RXVGA2B", 2, 2, value);
            PRINT_BITS("PD[1] RXVGA2A", 1, 1, value);
            PRINT_BITS("PD[0] Current reference", 0, 0, value);
            break;

        case 0x68:
            PRINT_BITS("VGA2GAINB", 7, 4, value);
            PRINT_BITS("VGA2GAINA", 3, 0, value);
            break;

        case 0x6e:
            PRINT_BITS("PD[7] DC cal comparitor for VGA2B", 7, 7, value);
            PRINT_BITS("PD[6] DC cal comparitor for VGA2A", 6, 6, value);
            PRINT_BITS("SPARE0[5:0]", 5, 0, value);
            break;

        case 0x6f:
            PRINT_BITS("SPARE1[7:0]", 7, 0, value);
            break;

        case 0x70:
            PRINT_BITS("Not used", 7, 2, value);
            PRINT_BITS("DECODE", 1, 1, value);
            PRINT_BITS("EN_RXFE", 0, 0, value);
            break;

        case 0x71:
            PRINT_BITS("IN1SEL_MIX_RXFE", 7, 7, value);
            PRINT_BITS("DCOFF_I_RXFE[6:0]", 6, 0, value);
            break;

        case 0x72:
            PRINT_BITS("INLOAD_LNA_RXFE", 7, 7, value);
            PRINT_BITS("DCOFF_Q_RXFE[6:0]", 6, 0, value);
            break;

        case 0x73:
            PRINT_BITS("XLOAD_LNA_RXFE", 7, 7, value);
            PRINT_BITS("IP2TRIM_I_RXFE[6:0]", 6, 0, value);
            break;

        case 0x74:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("IP2TRIM_Q_RXFE[6:0]", 6, 0, value);
            break;

        case 0x75:
            PRINT_BITS("G_LNA_RXFE[1:0]", 7, 6, value);
            PRINT_BITS("LNASEL_RXFE[1:0]", 5, 4, value);
            PRINT_BITS("CBE_LNA_RXFE[3:0]", 3, 0, value);
            break;

        case 0x76:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("RFB_TIA_RXFE[6:0]", 6, 0, value);
            break;

        case 0x77:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("CFB_TIA_RXFE[6:0]", 6, 0, value);
            break;

        case 0x78:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("RDLEXT_LNA_RXFE[5:0]", 5, 0, value);
            break;

        case 0x79:
            PRINT_BITS("Not used", 7, 6, value);
            PRINT_BITS("RDLINT_LNA_RXFE[5:0]", 5, 0, value);
            break;

        case 0x7a:
            PRINT_BITS("ICT_MIX_RXFE[3:0]", 7, 4, value);
            PRINT_BITS("ICT_LNA_RXFE[3:0]", 3, 0, value);
            break;

        case 0x7b:
            PRINT_BITS("ICT_TIA_RXFE[3:0]", 7, 4, value);
            PRINT_BITS("ICT_MXLOB_RXFE[3:0]", 3, 0, value);
            break;

        case 0x7c:
            PRINT_BITS("Not used", 7, 7, value);
            PRINT_BITS("LOBN_MIX_RXFE[3:0]", 6, 3, value);
            PRINT_BITS("RINEN_MIX_RXFE", 2, 2, value);
            PRINT_BITS("G_FINE_LNA3_RXFE[1:0]", 1, 0, value);
            break;

        case 0x7d:
            PRINT_BITS("Not used", 7, 4, value);
            PRINT_BITS("Reserved", 3, 0, value);
            break;

        case 0x7e:
            PRINT_BITS("SPARE0[7:0]", 7, 0, value);
            break;

        case 0x7f:
            PRINT_BITS("SPARE1[7:0]", 7, 0, value);
            break;

        default:
            break;
    }
}
