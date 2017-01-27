/*
 * Copyright (C) 2015 Nuand LLC
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

/* This file provides some compatibility items to ease porting code from
 * libbladeRF to the bladeRF NIO II code */

#ifndef LIBBLADERF_COMPAT_H_
#define LIBBLADERF_COMPAT_H_

#include "libbladeRF.h"
#include "debug.h"

/* Use our debug assert */
#ifndef BLADERF_NIOS_PC_SIMULATION
#   define assert ASSERT
#else
#   define ASSERT assert
#endif

/* libbladeRF code uses a FIELD_INIT macro as an MSVC workaround */
#define FIELD_INIT(param, val) param = val

/* For >= 1.5 GHz uses the high band should be used. Otherwise, the low
 * band should be selected */
#define BLADERF_BAND_HIGH (1500000000)

/* Replace log_* functions with existing DBG macro */
#define log_verbose(...) DBG(__VA_ARGS__)
#define log_debug(...)   DBG(__VA_ARGS__)
#define log_info(...)    DBG(__VA_ARGS__)
#define log_warning(...) DBG(__VA_ARGS__)
#define log_error(...)   DBG(__VA_ARGS__)
#define log_fatal(...)   DBG(__VA_ARGS__)

/* NIOS II builds don't need a device handle. Just forward-declare it to avoid
 * complaints from unused functions */
struct bladerf;

/* Associate macros with our NIO II implementations */
#   define LMS_WRITE(dev, addr, data) ({ \
        lms6_write(addr, data); \
        0; /* "Return" 0 */ \
    })

#   define LMS_READ(dev, addr, data_ptr) ({ \
        *(data_ptr) = lms6_read(addr); \
        0; /* "Return" 0 */ \
    })

#   define CONFIG_GPIO_READ(dev, data_ptr) ({ \
        *(data_ptr) = control_reg_read(); \
        0; /* "Return" 0 */ \
    })

#   define CONFIG_GPIO_WRITE(dev, data) ({ \
        control_reg_write(data); \
        0; /* "Return" 0 */ \
    })

#endif

