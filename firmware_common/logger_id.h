/*
 * Copyright (c) 2015 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LOGGER_ID_H_
#define LOGGER_ID_H_

#define LOGGER_ID_NONE                    0
#define LOGGER_ID_BLADERF_C               1
#define LOGGER_ID_FLASH_C                 2
#define LOGGER_ID_FPGA_C                  3
#define LOGGER_ID_GPIF_C                  4
#define LOGGER_ID_LOGGER_C                5
#define LOGGER_ID_RF_C                    6
#define LOGGER_ID_SPI_FLASH_LIB_C         7

#ifdef LOGGER_ID_STRING
static inline const char * logger_id_string(uint8_t file_id)
{
    switch (file_id) {
        case LOGGER_ID_NONE:
            return "<None>";
        case LOGGER_ID_BLADERF_C:
            return "bladeRF.c";
        case LOGGER_ID_FLASH_C:
            return "flash.c";
        case LOGGER_ID_FPGA_C:
            return "fpga.c";
        case LOGGER_ID_GPIF_C:
            return "gpif.c";
        case LOGGER_ID_LOGGER_C:
            return "logger.c";
        case LOGGER_ID_RF_C:
            return "rf.c";
        case LOGGER_ID_SPI_FLASH_LIB_C:
            return "spi_flash_lib.c";
        default:
            return "<Unknown>";
    }
}
#endif

#endif
