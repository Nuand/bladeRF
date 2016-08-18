/**
 * @brief   Normalizes the input IQ signal [-1.0, 1.0] to a power of 1.0
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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "host_config.h"

#include "pnorm.h"

struct pnorm_state_t {
    float est ;            //Estimate of signal power (essentially a running average)
    bool hold ;            //Hold the current gain?
    float alpha ;
    float invalpha ;
    float min_gain ;
    float max_gain ;
} ;

struct pnorm_state_t *pnorm_init(float alpha, float min_gain, float max_gain) {
    struct pnorm_state_t *state ;
    state = malloc(sizeof(state[0])) ;
    if( state == NULL ) {
        perror("malloc") ;
        return NULL ;
    }
    state->est = 1.0f ;
    state->hold = false ;
    state->alpha = alpha ;
    state->invalpha = (1.0f - alpha) ;

    assert(min_gain < max_gain) ;
    state->min_gain = min_gain ;
    state->max_gain = max_gain ;

    return state ;
}

void pnorm_reset(struct pnorm_state_t *state) {
    state->est = 1.0f ;
    state->hold = false ;
}

void pnorm_deinit(struct pnorm_state_t *state) {
    free(state) ;
    return ;
}

void pnorm_hold(struct pnorm_state_t *state, bool val) {
    state->hold = val ;
    return ;
}

void pnorm(struct pnorm_state_t *state, uint16_t length, struct complex_sample *in,
            struct complex_sample *out, float *ests, float *gains) {
    int i ;
    float power;
    int32_t temp;
    float gain, est;

    for( i = 0 ; i < length ; i++ ) {
        /* Power IIR filter */
        if( state->hold == false ) {
            //New estimate of power (normalized)
            est = state->est = state->alpha*state->est +
                state->invalpha*(in[i].i*in[i].i + in[i].q*in[i].q)/(SAMP_MAX_ABS*SAMP_MAX_ABS);
        } else {
            est = state->est ;
        }

        /* Ideal power is 1.0, so to get x to 1.0, we need to multiply by 1/est */
        gain = 1.0f/sqrtf(state->est) ;

        /* Check below min gain */
        if( gain < state->min_gain ) {
            gain = state->min_gain ;
        }

        /* Check above max gain */
        if( gain > state->max_gain ) {
            gain = state->max_gain ;
        }

        /* Apply gain */
        //Use temp int32_t in case this number goes outside 16bit range
        temp = (int32_t) round(in[i].i * gain);
        //Clamp
        if (temp > CLAMP_VAL_ABS){
            temp = CLAMP_VAL_ABS;
        }else if (temp < -CLAMP_VAL_ABS){
            temp = -CLAMP_VAL_ABS;
        }
        out[i].i = (int16_t) temp;

        temp = (int32_t) round(in[i].q * gain);
        //Clamp
        if (temp > CLAMP_VAL_ABS){
            temp = CLAMP_VAL_ABS;
        }else if (temp < -CLAMP_VAL_ABS){
            temp = -CLAMP_VAL_ABS;
        }
        out[i].q = (int16_t) temp;

        /* Blank impulse power */
        power = (out[i].i * out[i].i + out[i].q * out[i].q)/(float)(SAMP_MAX_ABS*SAMP_MAX_ABS);
        if (power >= 10.0f) {
            out[i].i = out[i].q = 0;
        }

        //Write to debug buffers
        if (ests != NULL){
            ests[i] = est;
        }
        if (gains != NULL){
            gains[i] = gain;
        }
    }
    return ;
}

#ifdef PNORM_TEST

#define NUM_SAMPLES 4000

struct complex_sample input[NUM_SAMPLES] ;
struct complex_sample output[NUM_SAMPLES] ;
float est[NUM_SAMPLES] ;
float gain[NUM_SAMPLES] ;

int main(int argc, char *argv[]) {
    FILE *fin, *fout ;
    struct pnorm_state_t *state ;
    float alpha, min_gain, max_gain ;
    unsigned int count, i ;
    int result;

    if( argc < 6 ) {
        fprintf(stderr, "Usage: %s <alpha> <min gain> <max gain> <input csv> <output csv>\n", argv[0]) ;
        return 1 ;
    }

    fin = fopen( argv[4], "r" ) ;
    if( fin == NULL ) {
        fprintf( stderr, "Couldn't open %s\n for input csv",argv[2] ) ;
        return 1 ;
    }

    fout = fopen( argv[5], "w" ) ;
    if( fout == NULL ) {
        fprintf( stderr, "Couldn't open %s for output csv", argv[3] ) ;
    }

    alpha = (float) atof(argv[1]) ;
    min_gain = (float) atof(argv[2]) ;
    max_gain = (float) atof(argv[3]) ;

    state = pnorm_init(alpha, min_gain, max_gain) ;
    while (!feof(fin)) {
        count = 0 ;
        while( count < sizeof(input) && !feof(fin) ) {
            result = fscanf( fin, "%hi,%hi\n", &input[count].i, &input[count].q ) ;
            if (result == EOF){
                break;
            }
            count++ ;
        }
        pnorm( state, count, input, output, est, gain ) ;
        for( i = 0 ; i < count ; i++ ) {
            fprintf( fout, "%hi,%hi,%15.9f,%15.9f\n", output[i].i,
                    output[i].q, est[i], gain[i] ) ;
        }
    }

    pnorm_deinit(state) ;

    fclose( fin ) ;
    fclose( fout ) ;

    return 0 ;
}

#endif

