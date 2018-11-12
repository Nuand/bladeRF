/***************************************************************************//**
 *   @file   dac_core.c
 *   @brief  Implementation of DAC Core Driver.
 *   @author DBogdan (dragos.bogdan@analog.com)
********************************************************************************
 * Copyright 2013(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include <stdint.h>

#include "dac_core.h"
#include "platform.h"
#include "util.h"

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
const uint16_t sine_lut[32] = {
		0x000, 0x031, 0x061, 0x08D, 0x0B4, 0x0D4, 0x0EC, 0x0FA,
		0x0FF, 0x0FA, 0x0EC, 0x0D4, 0x0B4, 0x08D, 0x061, 0x031,
		0x000, 0xFCE, 0xF9E, 0xF72, 0xF4B, 0xF2B, 0xF13, 0xF05,
		0xF00, 0xF05, 0xF13, 0xF2B, 0xF4B, 0xF72, 0xF9E, 0xFCE
};

/***************************************************************************//**
 * @brief dac_read
*******************************************************************************/
int dac_read(struct ad9361_rf_phy *phy, uint32_t regAddr, uint32_t *data)
{
	return axiadc_read(phy->adc_state, regAddr + 0x4000, data);
}

/***************************************************************************//**
 * @brief dac_write
*******************************************************************************/
int dac_write(struct ad9361_rf_phy *phy, uint32_t regAddr, uint32_t data)
{
	return axiadc_write(phy->adc_state, regAddr + 0x4000, data);
}

/***************************************************************************//**
 * @brief dds_default_setup
*******************************************************************************/
static int dds_default_setup(struct ad9361_rf_phy *phy,
							 uint32_t chan, uint32_t phase,
							 uint32_t freq, int32_t scale)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;

	ret = dds_set_phase(phy, chan, phase);
	if (ret < 0) {
		return ret;
	}

	ret = dds_set_frequency(phy, chan, freq);
	if (ret < 0) {
		return ret;
	}

	ret = dds_set_scale(phy, chan, scale);
	if (ret < 0) {
		return ret;
	}

	dds_st->cached_freq[chan] = freq;
	dds_st->cached_phase[chan] = phase;
	dds_st->cached_scale[chan] = scale;

	return 0;
}

/***************************************************************************//**
 * @brief dac_stop
*******************************************************************************/
int dac_stop(struct ad9361_rf_phy *phy)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) < 8)
	{
		return dac_write(phy, DAC_REG_CNTRL_1, 0);
	}

	return 0;
}

/***************************************************************************//**
 * @brief dac_start_sync
*******************************************************************************/
int dac_start_sync(struct ad9361_rf_phy *phy, bool force_on)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) < 8)
	{
		return dac_write(phy, DAC_REG_CNTRL_1, (dds_st->enable || force_on) ? DAC_ENABLE : 0);
	}
	else
	{
		return dac_write(phy, DAC_REG_CNTRL_1, DAC_SYNC);
	}
}

/***************************************************************************//**
 * @brief dac_init
*******************************************************************************/
int dac_init(struct ad9361_rf_phy *phy, uint8_t data_sel, uint8_t config_dma)
{
	struct dds_state *dds_st;
	int ret;
	uint32_t reg_ctrl_2;

	dds_st = &phy->adc_state->dac_dds_state;

	ret = dac_write(phy, DAC_REG_RSTN, 0x0);
	if (ret < 0) {
		return ret;
	}

	ret = dac_write(phy, DAC_REG_RSTN, DAC_RSTN);
	if (ret < 0) {
		return ret;
	}

	dds_st->dac_clk = &phy->clks[TX_SAMPL_CLK]->rate;
	dds_st->rx2tx2 = phy->pdata->rx2tx2;

	ret = dac_read(phy, DAC_REG_CNTRL_2, &reg_ctrl_2);
	if (ret < 0) {
		return ret;
	}

	if(dds_st->rx2tx2)
	{
		dds_st->num_dds_channels = 8;

		if(phy->pdata->port_ctrl.pp_conf[2] & LVDS_MODE)
			ret = dac_write(phy, DAC_REG_RATECNTRL, DAC_RATE(3));
		else
			ret = dac_write(phy, DAC_REG_RATECNTRL, DAC_RATE(1));

		if (ret < 0) {
			return ret;
		}

		reg_ctrl_2 &= ~DAC_R1_MODE;
	}
	else
	{
		dds_st->num_dds_channels = 4;

		if(phy->pdata->port_ctrl.pp_conf[2] & LVDS_MODE)
			ret = dac_write(phy, DAC_REG_RATECNTRL, DAC_RATE(1));
		else
			ret = dac_write(phy, DAC_REG_RATECNTRL, DAC_RATE(0));

		if (ret < 0) {
			return ret;
		}

		reg_ctrl_2 |= DAC_R1_MODE;
	}

	ret = dac_write(phy, DAC_REG_CNTRL_2, reg_ctrl_2);
	if (ret < 0) {
		return ret;
	}

	ret = dac_read(phy, DAC_REG_VERSION, &dds_st->pcore_version);
	if (ret < 0) {
		return ret;
	}

	ret = dac_write(phy, DAC_REG_CNTRL_1, 0);
	if (ret < 0) {
		return ret;
	}

	switch (data_sel) {
	case DATA_SEL_DDS:
		ret = dds_default_setup(phy, DDS_CHAN_TX1_I_F1, 90000, 1000000, 250000);
		if (ret < 0) {
			return ret;
		}

		ret = dds_default_setup(phy, DDS_CHAN_TX1_I_F2, 90000, 1000000, 250000);
		if (ret < 0) {
			return ret;
		}

		ret = dds_default_setup(phy, DDS_CHAN_TX1_Q_F1, 0, 1000000, 250000);
		if (ret < 0) {
			return ret;
		}

		ret = dds_default_setup(phy, DDS_CHAN_TX1_Q_F2, 0, 1000000, 250000);
		if (ret < 0) {
			return ret;
		}

		if(dds_st->rx2tx2)
		{
			ret = dds_default_setup(phy, DDS_CHAN_TX2_I_F1, 90000, 1000000, 250000);
			if (ret < 0) {
				return ret;
			}

			ret = dds_default_setup(phy, DDS_CHAN_TX2_I_F2, 90000, 1000000, 250000);
			if (ret < 0) {
				return ret;
			}

			ret = dds_default_setup(phy, DDS_CHAN_TX2_Q_F1, 0, 1000000, 250000);
			if (ret < 0) {
				return ret;
			}

			ret = dds_default_setup(phy, DDS_CHAN_TX2_Q_F2, 0, 1000000, 250000);
			if (ret < 0) {
				return ret;
			}
		}

		ret = dac_datasel(phy, -1, DATA_SEL_DDS);
		if (ret < 0) {
			return ret;
		}

		break;
	case DATA_SEL_DMA:
		ret = dac_datasel(phy, -1, DATA_SEL_DMA);
		if (ret < 0) {
			return ret;
		}
		break;
	default:
		break;
	}

	dds_st->enable = true;

	ret = dac_start_sync(phy, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_set_frequency
*******************************************************************************/
int dds_set_frequency(struct ad9361_rf_phy *phy, uint32_t chan, uint32_t freq)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint64_t val64;
	uint32_t reg;

	dds_st->cached_freq[chan] = freq;

	ret = dac_stop(phy);
	if (ret < 0) {
		return ret;
	}

	ret = dac_read(phy, DAC_REG_CHAN_CNTRL_2_IIOCHAN(chan), &reg);
	if (ret < 0) {
		return ret;
	}

	reg &= ~DAC_DDS_INCR(~0);
	val64 = (uint64_t) freq * 0xFFFFULL;
	do_div(&val64, *dds_st->dac_clk);
	reg |= DAC_DDS_INCR(val64) | 1;

	ret = dac_write(phy, DAC_REG_CHAN_CNTRL_2_IIOCHAN(chan), reg);
	if (ret < 0) {
		return ret;
	}

	ret = dac_start_sync(phy, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_set_phase
*******************************************************************************/
int dds_set_phase(struct ad9361_rf_phy *phy, uint32_t chan, uint32_t phase)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint64_t val64;
	uint32_t reg;

	dds_st->cached_phase[chan] = phase;

	ret = dac_stop(phy);
	if (ret < 0) {
		return ret;
	}

	ret = dac_read(phy, DAC_REG_CHAN_CNTRL_2_IIOCHAN(chan), &reg);
	if (ret < 0) {
		return ret;
	}

	reg &= ~DAC_DDS_INIT(~0);
	val64 = (uint64_t) phase * 0x10000ULL + (360000 / 2);
	do_div(&val64, 360000);
	reg |= DAC_DDS_INIT(val64);

	ret = dac_write(phy, DAC_REG_CHAN_CNTRL_2_IIOCHAN(chan), reg);
	if (ret < 0) {
		return ret;
	}

	ret = dac_start_sync(phy, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_set_phase
*******************************************************************************/
int dds_set_scale(struct ad9361_rf_phy *phy, uint32_t chan, int32_t scale_micro_units)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint32_t scale_reg;
	uint32_t sign_part;
	uint32_t int_part;
	uint32_t fract_part;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) > 6)
	{
		if(scale_micro_units >= 1000000)
		{
			sign_part = 0;
			int_part = 1;
			fract_part = 0;
			dds_st->cached_scale[chan] = 1000000;
			goto set_scale_reg;
		}
		if(scale_micro_units <= -1000000)
		{
			sign_part = 1;
			int_part = 1;
			fract_part = 0;
			dds_st->cached_scale[chan] = -1000000;
			goto set_scale_reg;
		}
		dds_st->cached_scale[chan] = scale_micro_units;
		if(scale_micro_units < 0)
		{
			sign_part = 1;
			int_part = 0;
			scale_micro_units *= -1;
		}
		else
		{
			sign_part = 0;
			int_part = 0;
		}
		fract_part = (uint32_t)(((uint64_t)scale_micro_units * 0x4000) / 1000000);
	set_scale_reg:
		scale_reg = (sign_part << 15) | (int_part << 14) | fract_part;
	}
	else
	{
		if(scale_micro_units >= 1000000)
		{
			scale_reg = 0;
			scale_micro_units = 1000000;
		}
		if(scale_micro_units <= 0)
		{
			scale_reg = 0;
			scale_micro_units = 0;
		}
		dds_st->cached_scale[chan] = scale_micro_units;
		fract_part = (uint32_t)(scale_micro_units);
		scale_reg = 500000 / fract_part;
	}

	ret = dac_stop(phy);
	if (ret < 0) {
		return ret;
	}

	ret = dac_write(phy, DAC_REG_CHAN_CNTRL_1_IIOCHAN(chan), DAC_DDS_SCALE(scale_reg));
	if (ret < 0) {
		return ret;
	}

	ret = dac_start_sync(phy, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_update
*******************************************************************************/
int dds_update(struct ad9361_rf_phy *phy)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint32_t chan;

	for(chan = DDS_CHAN_TX1_I_F1; chan <= DDS_CHAN_TX2_Q_F2; chan++)
	{
		ret = dds_set_frequency(phy, chan, dds_st->cached_freq[chan]);
		if (ret < 0) {
			return ret;
		}

		ret = dds_set_phase(phy, chan, dds_st->cached_phase[chan]);
		if (ret < 0) {
			return ret;
		}

		ret = dds_set_scale(phy, chan, dds_st->cached_scale[chan]);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

/***************************************************************************//**
 * @brief dac_datasel
*******************************************************************************/
int dac_datasel(struct ad9361_rf_phy *phy, int32_t chan, enum dds_data_select sel)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) > 7) {
		if (chan < 0) { /* ALL */
			uint32_t i;
			for (i = 0; i < dds_st->num_dds_channels; i++) {
				ret = dac_write(phy, DAC_REG_CHAN_CNTRL_7(i), sel);
				if (ret < 0) {
					return ret;
				}
			}
		} else {
			ret = dac_write(phy, DAC_REG_CHAN_CNTRL_7(chan), sel);
			if (ret < 0) {
				return ret;
			}
		}
	} else {
		uint32_t reg;

		switch(sel) {
		case DATA_SEL_DDS:
		case DATA_SEL_SED:
		case DATA_SEL_DMA:
			ret = dac_read(phy, DAC_REG_CNTRL_2, &reg);
			if (ret < 0) {
				return ret;
			}

			reg &= ~DAC_DATA_SEL(~0);
			reg |= DAC_DATA_SEL(sel);

			ret = dac_write(phy, DAC_REG_CNTRL_2, reg);
			if (ret < 0) {
				return ret;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_to_signed_mag_fmt
*******************************************************************************/
uint32_t dds_to_signed_mag_fmt(int32_t val, int32_t val2)
{
	uint32_t i;
	uint64_t val64;

	/* format is 1.1.14 (sign, integer and fractional bits) */

	switch (val) {
	case 1:
		i = 0x4000;
		break;
	case -1:
		i = 0xC000;
		break;
	case 0:
		i = 0;
		if (val2 < 0) {
				i = 0x8000;
				val2 *= -1;
		}
		break;
	default:
		/* Invalid Value */
		i = 0;
	}

	val64 = (uint64_t)val2 * 0x4000UL + (1000000UL / 2);
	do_div(&val64, 1000000UL);

	return i | (uint32_t)val64;
}

/***************************************************************************//**
 * @brief dds_from_signed_mag_fmt
*******************************************************************************/
void dds_from_signed_mag_fmt(uint32_t val,
							 int32_t *r_val,
							 int32_t *r_val2)
{
	uint64_t val64;
	int32_t sign;

	if (val & 0x8000)
		sign = -1;
	else
		sign = 1;

	if (val & 0x4000)
		*r_val = 1 * sign;
	else
		*r_val = 0;

	val &= ~0xC000;

	val64 = val * 1000000ULL + (0x4000 / 2);
	do_div(&val64, 0x4000);

	if (*r_val == 0)
		*r_val2 = (uint32_t)val64 * sign;
	else
		*r_val2 = (uint32_t)val64;
}

/***************************************************************************//**
 * @brief dds_set_calib_scale_phase
*******************************************************************************/
int32_t dds_set_calib_scale_phase(struct ad9361_rf_phy *phy,
								  uint32_t phase,
								  uint32_t chan,
								  int32_t val,
								  int32_t val2)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint32_t reg;
	uint32_t i;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) < 8) {
		return -1;
	}

	i = dds_to_signed_mag_fmt(val, val2);

	ret = dac_read(phy, DAC_REG_CHAN_CNTRL_8(chan), &reg);
	if (ret < 0) {
		return ret;
	}

	if (!((chan + phase) % 2)) {
		reg &= ~DAC_IQCOR_COEFF_1(~0);
		reg |= DAC_IQCOR_COEFF_1(i);
	} else {
		reg &= ~DAC_IQCOR_COEFF_2(~0);
		reg |= DAC_IQCOR_COEFF_2(i);
	}

	ret = dac_write(phy, DAC_REG_CHAN_CNTRL_8(chan), reg);
	if (ret < 0) {
		return ret;
	}

	ret = dac_write(phy, DAC_REG_CHAN_CNTRL_6(chan), DAC_IQCOR_ENB);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief dds_get_calib_scale_phase
*******************************************************************************/
int32_t dds_get_calib_scale_phase(struct ad9361_rf_phy *phy,
								  uint32_t phase,
								  uint32_t chan,
								  int32_t *val,
								  int32_t *val2)
{
	struct dds_state *dds_st = &phy->adc_state->dac_dds_state;
	int ret;
	uint32_t reg;

	if (PCORE_VERSION_MAJOR(dds_st->pcore_version) < 8) {
		return -1;
	}

	ret = dac_read(phy, DAC_REG_CHAN_CNTRL_8(chan), &reg);
	if (ret < 0) {
		return ret;
	}

	/* format is 1.1.14 (sign, integer and fractional bits) */

	if (!((phase + chan) % 2)) {
		reg = DAC_TO_IQCOR_COEFF_1(reg);
	} else {
		reg = DAC_TO_IQCOR_COEFF_2(reg);
	}

	dds_from_signed_mag_fmt(reg, val, val2);

	return 0;
}

/***************************************************************************//**
 * @brief dds_set_calib_scale
*******************************************************************************/
int32_t dds_set_calib_scale(struct ad9361_rf_phy *phy,
							uint32_t chan,
							int32_t val,
							int32_t val2)
{
	return dds_set_calib_scale_phase(phy, 0, chan, val, val2);
}

/***************************************************************************//**
 * @brief dds_get_calib_scale
*******************************************************************************/
int32_t dds_get_calib_scale(struct ad9361_rf_phy *phy,
							uint32_t chan,
							int32_t *val,
							int32_t *val2)
{
	return dds_get_calib_scale_phase(phy, 0, chan, val, val2);
}

/***************************************************************************//**
 * @brief dds_set_calib_phase
*******************************************************************************/
int32_t dds_set_calib_phase(struct ad9361_rf_phy *phy,
							uint32_t chan,
							int32_t val,
							int32_t val2)
{
	return dds_set_calib_scale_phase(phy, 1, chan, val, val2);
}

/***************************************************************************//**
 * @brief dds_get_calib_phase
*******************************************************************************/
int32_t dds_get_calib_phase(struct ad9361_rf_phy *phy,
							uint32_t chan,
							int32_t *val,
							int32_t *val2)
{
	return dds_get_calib_scale_phase(phy, 1, chan, val, val2);
}
