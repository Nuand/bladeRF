/**
 * @brief   Utility functions: loading/saving samples from/to a file,
 *          conversions, etc.
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

#include "utils.h"

int load_samples_from_csv_file(char *filename, bool pad_zeros, int buffer_size,
                                int16_t **samples)
{
    int i, result, num_samples;
    int samples_buf_size;        //Max number of samples in buffer
    int buf_inc_size;            //Number of additional samples to add space for on a realloc()
    bool reachedEOF;
    int16_t temp_sample1, temp_sample2;
    int16_t *tmp_ptr;

    //Open csv file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL){
        perror("fopen");
        *samples = NULL;
        return -1;
    }

    if (pad_zeros){
        buf_inc_size = buffer_size;
    }else{
        buf_inc_size = 1024;
    }

    //Allocate initial memory for tx_samples
    *samples = (int16_t *) malloc(buf_inc_size * 2 * sizeof(int16_t));
    if (*samples == NULL){
        perror("malloc");
        fclose(fp);
        return -1;
    }

    //Initial size of allocated memory
    samples_buf_size = buf_inc_size;

    //Read in samples, and reallocate more memory if needed
    i = 0;
    num_samples = 0;
    reachedEOF = false;
    while(true){
        //Check to see if the samples buffer is full
        if (num_samples == samples_buf_size){
            //Before allocating more memory, check to see if there are no more samples
            result = fscanf(fp, "%hi,%hi\n", &temp_sample1, &temp_sample2 );
            if(result == EOF || reachedEOF){
                break;
            }
            //allocate more memory. Add space for another 'buf_inc_size' samples
            samples_buf_size += buf_inc_size;
            tmp_ptr = (int16_t *) realloc(*samples, samples_buf_size * 2 * sizeof(int16_t));
            if (tmp_ptr == NULL){
                perror("realloc");
                free(*samples);
                *samples = NULL;
                fclose(fp);
                return -1;
            }
            *samples = tmp_ptr;
            (*samples)[i] = temp_sample1;
            (*samples)[i+1] = temp_sample2;
            num_samples++;
            i += 2;
            continue;
        }
        if (!reachedEOF){
            result = fscanf(fp, "%hi,%hi\n", &(*samples)[i], &(*samples)[i+1] );
            if (result == EOF){
                reachedEOF = true;
            }
        }
        if (reachedEOF){
            if (pad_zeros){
                //pad with zeros to fill the rest of the buffer
                (*samples)[i] = 0;
                (*samples)[i+1] = 0;
            }else{
                break;
            }
        }
        num_samples++;
        i += 2;        //increment index
    }
    fclose(fp);

    return num_samples;
}

int load_struct_samples_from_csv_file(char *filename, bool pad_zeros, int buffer_size,
                                        struct complex_sample **samples)
{
    int i, result;
    int samples_buf_size;        //Max number of samples in buffer
    int buf_inc_size;            //Number of additional samples to add space for on a realloc()
    bool reachedEOF;
    int16_t temp_sample1, temp_sample2;
    struct complex_sample *tmp_ptr;

    //Open csv file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL){
        perror("fopen");
        *samples = NULL;
        return -1;
    }

    if (pad_zeros){
        buf_inc_size = buffer_size;
    }else{
        buf_inc_size = 1024;
    }

    //Allocate initial memory for tx_samples
    *samples = (struct complex_sample *) malloc(buf_inc_size * sizeof(struct complex_sample));
    if (*samples == NULL){
        perror("malloc");
        fclose(fp);
        return -1;
    }

    //Initial size of allocated memory
    samples_buf_size = buf_inc_size;

    //Read in samples, and reallocate more memory if needed
    i = 0;
    reachedEOF = false;
    while(true){
        //Check to see if the samples buffer is full
        if (i == samples_buf_size){
            //Before allocating more memory, check to see if there are no more samples
            result = fscanf(fp, "%hi,%hi\n", &temp_sample1, &temp_sample2 );
            if(result == EOF || reachedEOF){
                break;
            }
            //allocate more memory. Add space for another 'buf_inc_size' samples
            samples_buf_size += buf_inc_size;
            tmp_ptr = (struct complex_sample *) realloc(*samples,
                                       samples_buf_size * sizeof(struct complex_sample));
            if (tmp_ptr == NULL){
                perror("realloc");
                free(*samples);
                *samples = NULL;
                fclose(fp);
                return -1;
            }
            *samples = tmp_ptr;
            (*samples)[i].i = temp_sample1;
            (*samples)[i].q = temp_sample2;
            i++;
            continue;
        }
        if (!reachedEOF){
            result = fscanf(fp, "%hi,%hi\n", &(*samples)[i].i, &(*samples)[i].q);
            if (result == EOF){
                reachedEOF = true;
            }
        }
        if (reachedEOF){
            if (pad_zeros){
                //pad with zeros to fill the rest of the buffer
                (*samples)[i].i = 0;
                (*samples)[i].q = 0;
            }else{
                break;
            }
        }
        i++;        //increment index
    }
    fclose(fp);

    return i;
}

int write_samples_to_csv_file(char *filename, int16_t *samples, int num_samples)
{
    int i;
    //Open file
    FILE *fp = fopen(filename, "w");
    if (fp == NULL){
        perror("fopen");
        return -1;
    }
    //Write file
    //Loop through all samples
    //First int is the I sample. Second int is the Q sample
    for (i = 0; i < num_samples*2; i+= 2){
        fprintf(fp, "%hi,%hi\n", samples[i], samples[i+1]);
    }
    fclose(fp);

    return 0;
}

int write_struct_samples_to_csv_file(char *filename, struct complex_sample *samples,
                                        int num_samples)
{
    int i;
    //Open file
    FILE *fp = fopen(filename, "w");
    if (fp == NULL){
        perror("fopen");
        return -1;
    }
    //Write file
    //Loop through all samples
    //First int is the I sample. Second int is the Q sample
    for (i = 0; i < num_samples; i++){
        fprintf(fp, "%hi,%hi\n", samples[i].i, samples[i].q);
    }
    fclose(fp);

    return 0;
}

void print_chars(uint8_t *data, int num_bytes)
{
    int i;

    printf("'");
    for (i = 0; i < num_bytes; i++){
        printf("%c", data[i]);
    }
    printf("'\n");
}

void print_hex_contents(uint8_t *data, int num_bytes)
{
    int i;

    for (i = 0; i < num_bytes; i++){
        printf("%.2X ", data[i]);
    }
    printf("\n");
}

int create_timeout_abs(unsigned int timeout_ms, struct timespec *timeout_abs)
{
    int status;

    //Get the current time
    status = clock_gettime(CLOCK_REALTIME, timeout_abs);
    if(status != 0){
        perror("clock_gettime");
        return -1;
    }

    //Add the timeout onto the current time
    timeout_abs->tv_sec += timeout_ms/1000;
    timeout_abs->tv_nsec += (timeout_ms % 1000) * 1000000;
    //Check for overflow in nsec
    if (timeout_abs->tv_nsec >= 1000000000){
        timeout_abs->tv_sec += timeout_abs->tv_nsec / 1000000000;
        timeout_abs->tv_nsec %= 1000000000;
    }

    return 0;
}

void conv_samples_to_struct(int16_t *samples, unsigned int num_samples,
                            struct complex_sample *struct_samples)
{
    unsigned int i;

    for (i = 0; i < (num_samples*2); i += 2){
        struct_samples[i/2].i = samples[i];
        struct_samples[i/2].q = samples[i+1];
    }
}

void conv_struct_to_samples(struct complex_sample *struct_samples, unsigned int num_samples,
                            int16_t *samples)
{
    unsigned int i;

    for (i = 0; i < (num_samples*2); i += 2){
        samples[i] = struct_samples[i/2].i;
        samples[i+1] = struct_samples[i/2].q;
    }
}
