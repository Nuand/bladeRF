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
    uint64_t ProcessedSamples;
    int LastI;
    int LastQ;
    bool First;
    bool FirstBlock;
} Bench_data_t;

static void *bench_callback32(struct bladerf *dev,                         
struct bladerf_stream *stream,
struct bladerf_metadata *meta,
    void *samples,
    size_t num_samples,
    void *user_data)
{
    Bench_data_t *CBData = (Bench_data_t*) user_data;
    uint32_t *Data = (uint32_t*)samples;
    uint32_t Diff = Data[num_samples-1]-Data[0];
    int ExtraSamples ;

    if (CBData->FirstBlock)       //First block might still have some of the old counters in the block.. ignore it..
    {
        CBData->FirstBlock=false;
        return samples;
    }
    
    ExtraSamples = Diff - num_samples;
    if (ExtraSamples >0)
    {
        CBData->MissedSamples +=ExtraSamples;
    }
    CBData->Count--;
    CBData->ProcessedSamples+=num_samples;
    if (CBData->Count)
    {
        return samples;
    }
    else
    {
        return NULL;
    }
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
        bladerf_config_gpio_write(state->dev, GpioValue | (2<<8) );//Enable counting
        bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        for (i=1; i<=40; i++)
        {
            int BlockSize ;
            BW = (unsigned int) (i*1E6);
            BlockSize = (BW/10) & (~0xffff); //100ms blocks with 64k boundary
            if (BlockSize > 1024*1024)
                BlockSize = 1024*1024;
            bladerf_set_sample_rate(state->dev, BLADERF_MODULE_RX, (unsigned int)BW, &Actual); 
            Data.First = true;
            Data.FirstBlock = true;
            Data.LastI = 0;
            Data.LastQ = 0;
            Data.Count = Data.Blocks =  (int) (((((double)BW) / (BlockSize))*15)+2); //15sec or next closest..
            //printf("Speed : %dMhz Blocks = %d BlockSize = %d\n",i,Data.Count,BlockSize);
            Data.MissedSamples = 0;
            Data.ProcessedSamples=0;
            if (bladerf_init_stream(&Stream, state->dev, bench_callback32, &Buffers, 64, BLADERF_FORMAT_SC16_Q12, (size_t)(BlockSize), 64,&Data)==0)
            {
                bladerf_stream(Stream , BLADERF_MODULE_RX);
                bladerf_deinit_stream(Stream);
                printf("Speed : %dMhz processed = %d, missed = %d \n",(int)(BW/(1E6)) ,(int)Data.ProcessedSamples, Data.MissedSamples);
            }
            else
            {
                printf("Boo %dMhz\n",(int)(BW/1E6));
            }
        }
        bladerf_config_gpio_write(state->dev, GpioValue  );//Restore to original setting
        bladerf_enable_module(state->dev, BLADERF_MODULE_RX, false);
        printf("\n");
    }
    return CMD_RET_OK;
}

