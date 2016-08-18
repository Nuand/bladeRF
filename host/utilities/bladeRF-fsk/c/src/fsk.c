/**
 * @brief   FSK modulator/demodulator
 *
 * This file handles raw modulation and demodulation of bits using continuous-phase
 * frequency shift keying (CPFSK). fsk_mod() takes in an array of bytes and produces
 * a CPFSK signal in the form of baseband IQ samples. fsk_demod() takes in an array of
 * CPFSK IQ samples and produces an array of bytes.
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
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
#include "host_config.h"
#include "fsk.h"

#ifdef DEBUG_MODE
    #define DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
#else
    #define DEBUG_MSG(...)
#endif

//internal structs
struct fsk_handle {
    struct complex_sample *sample_table;
    int points_per_rev;
    int samp_per_symb;
    //These variables keep track of the demodulator's state when a call to fsk_demod()
    //did not fully demodulate the last byte, meaning it needs to be called again with
    //more samples to finish demodulating that last byte
    bool last_byte_demod_complete;  //False if the last byte was partially demodulated
                                    //in a call to fsk_demod()
    uint8_t last_byte;
    double last_phase;
    double curr_dphase_tot;
    int curr_samp_index;            //Current samples index (0 - SAMP_PER_SYMB-1)
    int curr_bit_index;
};

//internal functions
static struct complex_sample *fsk_gen_samples_table(int points_per_rev);
static double angle(int i, int q);
static void angle_unwrap(double angle_prev, double *angle);

/**
 * Generate 16bit two's complement IQ samples (SC16 Q11 format) corresponding to angles
 * from 0 to 2*pi on the unit circle. Range = [-2048, 2047]. Store in 'sample_table'.
 * 0th index is at 1+j0 (2047 + j0). Next indices will rotate counter-clockwise around 
 * the circle.
 * Example: points_per_rev = 12 -> would produce:
 *
 *           |Q
 *         . ' .
 *       .   |   .
 *    --.----+----.--I
 *       .   |   .
 *         . | .
 *           '
 *           |
 *
 * @param[in]   points_per_rev      number of points desired in one revolution (0-2*pi)
 *                                  of the complex unit circle
 *
 * @return      pointer to array of complex samples
 */
static struct complex_sample *fsk_gen_samples_table(int points_per_rev)
{
    int i;
    struct complex_sample *sample_table;

    sample_table = malloc(points_per_rev*sizeof(struct complex_sample));
    if (sample_table == NULL){
        perror("malloc");
        return NULL;
    }

    for (i = 0; i < points_per_rev; i++){
        //Calculate cosine/sine samples. Round to nearest integer.
        sample_table[i].i = (int16_t) round(2048.0*cos(i * 2.0 * M_PI / points_per_rev));
        sample_table[i].q = (int16_t) round(2048.0*sin(i * 2.0 * M_PI / points_per_rev));
        //Clamp values to [-2048, 2047]
        if (sample_table[i].i > 2047){
            sample_table[i].i = 2047;
        }
        if (sample_table[i].i < -2048){
            sample_table[i].i = -2048;
        }
        if (sample_table[i].q > 2047){
            sample_table[i].q = 2047;
        }
        if (sample_table[i].q < -2048){
            sample_table[i].q = -2048;
        }
    }
    return sample_table;
}

unsigned int fsk_mod(struct fsk_handle *fsk, uint8_t *data_buf, int num_bytes,
                        struct complex_sample *samples)
{

    int samp_table_pos;        //current position in samples table
    int byte;                //current byte (0-(num_bytes-1))
    int bit;                //current bit (0-7) in byte
    int samp;                //current sample in symbol period (0-(samps_per_symb-1))
    int i;                    //index in samples buffer

    //Set initial position to 0 (1 + 0j)
    samp_table_pos = 0;
    i = 0;
    for (byte = 0; byte < num_bytes; byte++){
        for (bit = 0; bit < 8; bit++){
            //Check for 1
            if ( ((data_buf[byte] >> bit) & 0x01) == 0x01 ){
                //This bit is a 1. Rotate phase CCW
                for (samp = 0; samp < fsk->samp_per_symb; samp++){
                    if (samp_table_pos == fsk->points_per_rev - 1){
                        samp_table_pos = 0;
                    }else{
                        samp_table_pos += 1;    //Increment phase
                    }
                    samples[i].i = fsk->sample_table[samp_table_pos].i;
                    samples[i].q = fsk->sample_table[samp_table_pos].q;
                    i++;
                }
            }else{
                //This bit is a 0. Rotate phase CW
                for (samp = 0; samp < fsk->samp_per_symb; samp++){
                    if (samp_table_pos == 0){
                        samp_table_pos = fsk->points_per_rev - 1;
                    }else{
                        samp_table_pos -= 1;    //Decrement phase
                    }
                    samples[i].i = fsk->sample_table[samp_table_pos].i;
                    samples[i].q = fsk->sample_table[samp_table_pos].q;
                    i++;
                }
            }
        }
    }
    return i;
}

/**
 * Unwrap a single angle so it is not wrapped to [-pi, pi]. Example: If angle_prev is 0.9*pi,
 * and (*angle) is -0.9*pi, then (*angle) will be changed to 1.1*pi, so the large
 * (discontinuous) phase jump is not seen.
 *
 * @param[in]       angle_prev      pointer to initial angle in radians
 * @param[inout]    angle           pointer to final angle. If an angle jump greater than pi
 *                                  or less than pi is found, this angle will be modified.
 */
static void angle_unwrap(double angle_prev, double *angle)
{
    double diff = *angle - angle_prev;
    if (diff > M_PI){
        //Subtract a multiple of 2*pi from the angle
        *angle -= 2*M_PI * (int)( (diff+M_PI)/(2*M_PI) );
    }else if (diff < -M_PI){
        //Add a multiple of 2*pi to the angle
        *angle += 2*M_PI * (int)( (diff-M_PI)/(-2*M_PI) );
    }
}

/**
 * Returns the angle of I+jQ, using the interval (-pi, pi)
 */
static double angle(int i, int q)
{
    double ratio = (double)q / (double)i;
    if (i == 0 && q == 0){
        return 0;    //Angle is undefined here. Just return 0.
    }else if (i >= 0 && q >= 0){
        return atan(ratio);
    }else if (i < 0 && q >= 0){
        return atan(ratio) + M_PI;
    }else if (i < 0 && q < 0){
        return atan(ratio) - M_PI;
    }else{    //if (i >= 0 && q < 0)
        return atan(ratio);
    }
}

unsigned int fsk_demod(struct fsk_handle *fsk, struct complex_sample *samples,
                    int num_samples, bool new_signal, int num_bytes, uint8_t *data_buf)
{
    int i;        //Current samples index (0 to num_samples-1)
    int byte = 0;
    int bit;
    int samp;
    double phase, phase_prev, dphase_tot;

    i = 0;
    if (new_signal){
        //Reset everything, calculate initial phase
        fsk->curr_samp_index = 0;
        fsk->curr_dphase_tot = 0;
        fsk->curr_bit_index = 0;
        fsk->last_phase = angle(samples[0].i, samples[0].q);
        i++;
    }
    phase = fsk->last_phase;
    //Initialize byte appropriately if last demod was not fully completed
    //(i.e. last byte was partially demodulated)
    if (!fsk->last_byte_demod_complete){
        data_buf[0] = fsk->last_byte;
    }
    //Loop through each byte
    for (byte = 0; byte != num_bytes && i < num_samples; byte++){
        //Loop through each bit in the byte
        for (bit = fsk->curr_bit_index; bit < 8; bit++){
            //Loop through SAMP_PER_SYMB samples and their corresponding changes in phase
            //Stop if we reach the end of the samples buffer
            dphase_tot = fsk->curr_dphase_tot;
            for (samp = fsk->curr_samp_index; (samp < fsk->samp_per_symb) && i < num_samples;
                    samp++){
                phase_prev = phase;
                phase = angle(samples[i].i, samples[i].q);
                //Unwrap angle
                angle_unwrap(phase_prev, &phase);
                //Add this angle change to the total angle change
                dphase_tot += phase - phase_prev;
                i++;
            }
            //Check to see if we broke out of the loop before demodulating the full bit
            if (samp != fsk->samp_per_symb){
                //Set demod state information
                fsk->last_byte_demod_complete = false;
                fsk->last_byte = data_buf[byte];
                fsk->last_phase = phase;
                fsk->curr_dphase_tot = dphase_tot;
                fsk->curr_samp_index = samp;
                fsk->curr_bit_index = bit;
                goto out;
            }
            if (dphase_tot > 0){
                //Received a 1. Set bit to 1.
                data_buf[byte] |= 0x01 << bit;
            }else{
                //Received a 0. Set bit to 0.
                data_buf[byte] &= ~(0x01 << bit);
            }
            //Reset index variables
            fsk->curr_samp_index = 0;
            fsk->curr_dphase_tot = 0;
        }
        fsk->last_phase = phase;
        fsk->curr_bit_index = 0;
        fsk->last_byte_demod_complete = true;
    }

    out:
        return byte;
}

struct fsk_handle *fsk_init(void)
{
    struct fsk_handle *fsk;

    //Allocate memory for handle
    fsk = malloc(sizeof(struct fsk_handle));
    if (fsk == NULL){
        perror("malloc");
        return NULL;
    }
    //Set modulation/demodulation parameters
    fsk->samp_per_symb = SAMP_PER_SYMB;
    fsk->points_per_rev = POINTS_PER_REV;
    //Generate the samples table
    fsk->sample_table = fsk_gen_samples_table(POINTS_PER_REV);
    if (fsk->sample_table == NULL){
        fprintf(stderr, "Couldn't generate samples table\n");
        free(fsk);
        return NULL;
    }
    //Initialize demod state variables
    fsk->last_byte_demod_complete = true;
    fsk->last_byte = 0x00;
    fsk->curr_dphase_tot = 0;
    fsk->last_phase = 0;
    fsk->curr_samp_index = 0;
    fsk->curr_bit_index = 0;
    return fsk;
}

void fsk_close(struct fsk_handle *fsk)
{
    if (fsk != NULL){
        free(fsk->sample_table);
    }
    free(fsk);
}
