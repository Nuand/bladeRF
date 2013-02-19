/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2006 Altera Corporation, San Jose, California, USA.           *
* All rights reserved.                                                        *
*                                                                             *
* Permission is hereby granted, free of charge, to any person obtaining a     *
* copy of this software and associated documentation files (the "Software"),  *
* to deal in the Software without restriction, including without limitation   *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
* and/or sell copies of the Software, and to permit persons to whom the       *
* Software is furnished to do so, subject to the following conditions:        *
*                                                                             *
* The above copyright notice and this permission notice shall be included in  *
* all copies or substantial portions of the Software.                         *
*                                                                             *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
* DEALINGS IN THE SOFTWARE.                                                   *
*                                                                             *
* This agreement shall be governed in all respects by the laws of the State   *
* of California and by the laws of the United States of America.              *
*                                                                             *
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
******************************************************************************/

#include <stdio.h>

#ifdef ALT_USE_DIRECT_DRIVERS
#include "system.h"
#include "sys/alt_driver.h"
#include "sys/alt_stdio.h"
#include "priv/alt_file.h"
#include "unistd.h"
#endif

/*
 * Uses the ALT_DRIVER_READ() macro to call directly to driver if available.
 * Otherwise, uses newlib provided getchar() routine.
 */
int 
alt_getchar(void)
{
#ifdef ALT_USE_DIRECT_DRIVERS
    ALT_DRIVER_READ_EXTERNS(ALT_STDIN_DEV);
    char c;

    if (ALT_DRIVER_READ(ALT_STDIN_DEV, &c, 1, alt_fd_list[STDIN_FILENO].fd_flags) <= 0) {
        return -1;
    }
    return c;
#else
    return getchar();
#endif
}
