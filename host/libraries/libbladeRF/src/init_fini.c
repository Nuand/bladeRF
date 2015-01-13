/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "conversions.h"
#include "version.h"
#if !defined(WIN32) && !defined(__CYGWIN__) && defined(LOG_SYSLOG_ENABLED)
#include <syslog.h>
#endif
#include "log.h"

#if !defined(WIN32) && !defined(__CYGWIN__)
#if !defined(__clang__) && !defined(__GNUC__)
#error init/fini mechanism not known to work for your compiler.
#endif
#define __init __attribute__((constructor))
#define __fini __attribute__((destructor))
#else
/* Corresponding syntax for Windows (TBD) */
#define __init
#define __fini
#endif

#ifdef LOG_SYSLOG_ENABLED
#   define DEF_LOG_LEVEL BLADERF_LOG_LEVEL_WARNING
#else
#   define DEF_LOG_LEVEL BLADERF_LOG_LEVEL_INFO
#endif
#define ENV_LOG_LEVEL "BLADERF_LOG_LEVEL"

/* Module initializers/deinitializers. When used as library (who don't have
 * a natural entry/exit function) these are used to initialize
 * deinitialize. Use to set predefined/default states and cleanup.
 *
 * This will work with shared libraries as well as with static as they get
 * invoked by RTL load/unload, with or without C++ code (i.e. functions will
 * play nice with C++ normal ctors/dtors).
 *
 * Keep log in to at least once per new build-/run-environment assert that
 * the mechanism works.
 */

static int get_loglevel(void) {
    int log_level = DEF_LOG_LEVEL;

    if ((getenv(ENV_LOG_LEVEL) != NULL)
        && (strlen(getenv(ENV_LOG_LEVEL)) > 0)) {

        bool valid_value;
        log_level = str2loglevel(getenv(ENV_LOG_LEVEL), &valid_value);

        if (!valid_value) {
            log_level = DEF_LOG_LEVEL;
        }
    }
    return log_level;
}

void __init __bladerf_init(void)
{
    int log_level = get_loglevel();

#if !defined(WIN32) && !defined(__CYGWIN__) && defined(LOG_SYSLOG_ENABLED)
    openlog("bladeRF",
            LOG_CONS | LOG_NDELAY | LOG_NOWAIT | LOG_PERROR | LOG_PID,
            LOG_USER);
#endif

    bladerf_log_set_verbosity(log_level);
    log_debug("libbladeRF %s: initializing\n", LIBBLADERF_VERSION);
}

void __fini __bladerf_fini(void)
{
    int log_level = get_loglevel();

    bladerf_log_set_verbosity(log_level);
    log_debug("libbladeRF %s: deinitializing\n", LIBBLADERF_VERSION);
    fflush(NULL);
#if !defined(WIN32) && !defined(__CYGWIN__) && defined(LOG_SYSLOG_ENABLED)
    closelog();
#endif
}
