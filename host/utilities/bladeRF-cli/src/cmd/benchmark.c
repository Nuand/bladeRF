/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
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
#include "cmd.h"
#include "version.h"
#include <stdio.h>

typedef struct 
{
    int Count;
    int Blocks;
    int MissedSamples;
    int LastI;
    int LastQ;
    bool First;
    bool FirstBlock;
} Bench_data_t;

static void *bench_callback(struct bladerf *dev,
                         struct bladerf_stream *stream,
                         struct bladerf_metadata *meta,
                         void *samples,
                         size_t num_samples,
                         void *user_data)
{
    unsigned int i;
    uint16_t *Data = (uint16_t*) samples;
    Bench_data_t *CBData = (Bench_data_t*) user_data;
    int Start=0;
    
    if(CBData->FirstBlock) //First block will have left over data in it.. lets never look at it..
    {
        CBData->FirstBlock =false;
        return samples;
    }

    if (CBData->First)
    {
        Start=1;
        CBData->LastI = Data[0];
        CBData->LastQ = Data[1];
        CBData->First = false;
    }
    
    CBData->Count--;

    for (i=Start; i<num_samples; i++)
    {
        int NewValue =(CBData->LastI+1) % 2048;
        if (NewValue != Data[i*2])
        {
            printf("Block %d Offset = %d Skip = %d\n",CBData->Blocks - CBData->Count,i,  (2048+Data[i*2]-NewValue) % 2048);
            CBData->MissedSamples++;
        }
        CBData->LastI = Data[i*2];
    }

    if (CBData->Count)
    {
        return samples;
    }
    else
        return NULL;
}

int cmd_benchmark(struct cli_state *state, int argc, char **argv)
{
    int status;
    
    bool fpga_loaded = false;

    /* Exit cleanly if no device is attached */
    if (state->dev == NULL) {
        printf("  Device version information unavailable: No device attached.\n");
        return 0;
    }

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    } else if (status != 0) {
        fpga_loaded = true;
    }
    if (fpga_loaded)
    {
        uint32_t Actual = 0;
        uint32_t GpioValue = 0;
        struct bladerf_stream *Stream;
        Bench_data_t Data;
        void **Buffers;
        unsigned int BW ;
        int i;

        printf("Starting bandwidth benchmark.\n");
        bladerf_config_gpio_read(state->dev,  &GpioValue);
        bladerf_config_gpio_write(state->dev, GpioValue | (1<<8) );//Enable counting
        bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        for (i=1; i<=40; i++)
        {
            BW = (unsigned int) (i*1E6);
            bladerf_set_sample_rate(state->dev, BLADERF_MODULE_RX, (unsigned int)BW, &Actual); 
            Data.First = true;
            Data.FirstBlock = true;
            Data.LastI = 0;
            Data.LastQ = 0;
            Data.Count = Data.Blocks =  (int) (((((double)BW) / (1024*1024))*5)+1); //5sec or next closest..
            Data.MissedSamples = 0;
            bladerf_init_stream(&Stream, state->dev, bench_callback, &Buffers, 64, BLADERF_FORMAT_SC16_Q12, (size_t)(1024*1024) ,64,&Data);
            bladerf_stream(Stream , BLADERF_MODULE_RX);
            bladerf_deinit_stream(Stream);
            printf("Speed : %dMhz missed = %d \n",(int)(BW/(1E6)) ,Data.MissedSamples);
        }
        bladerf_config_gpio_write(state->dev, GpioValue  );//Restore to original setting
        bladerf_enable_module(state->dev, BLADERF_MODULE_RX, false);
        printf("\n");
    }
    return CMD_RET_OK;
}

