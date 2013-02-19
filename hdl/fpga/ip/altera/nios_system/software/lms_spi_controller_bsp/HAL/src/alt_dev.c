/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2004 Altera Corporation, San Jose, California, USA.           *
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

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "sys/alt_dev.h"
#include "priv/alt_file.h"

#include "alt_types.h"

#include "system.h"

/*
 * This file contains the data constructs used to control access to device and
 * filesytems.
 */

/*
 * "alt_fs_list" is the head of a linked list of registered filesystems. It is 
 * initialised as an empty list. New entries can be added using the 
 * alt_fs_reg() function.  
 */

ALT_LLIST_HEAD(alt_fs_list);


/*
 * "alt_dev_list" is the head of a linked list of registered devices. It is
 * configured at startup to include a single device, "alt_dev_null". This
 * device is discussed below.
 */

extern alt_dev alt_dev_null; /* forward declaration */

alt_llist alt_dev_list = {&alt_dev_null.llist, &alt_dev_null.llist};

/*
 * alt_dev_null_write() is the implementation of the write() function used
 * by the alt_dev_null device. It simple discards all data passed to it, and
 * indicates that the data has been successfully transmitted.
 */

static int alt_dev_null_write (alt_fd* fd, const char* ptr, int len)
{
  return len;
}

/*
 * "alt_dev_null" is used to allow output to be redirected to nowhere. It is
 * the only device registered before the call to alt_sys_init(). At startup 
 * stin, stdout & stderr are all directed towards this device so that library 
 * calls like printf() will be safe but inefectual.  
 */

alt_dev alt_dev_null = {
   {
     &alt_dev_list,
     &alt_dev_list
   },
   "/dev/null",
   NULL,               /* open */
   NULL,               /* close */
   NULL,               /* write */
   alt_dev_null_write, /* write */
   NULL,               /* lseek */   
   NULL,               /* fstat */ 
   NULL                /* ioctl */
 };

/*
 * "alt_fd_list_lock" is a semaphore used to control access to the file
 * descriptor list. This is used to ensure that access to the list is thread
 * safe.  
 */

ALT_SEM(alt_fd_list_lock)

/*
 * "alt_max_fd" is used to make access to the file descriptor list more 
 * efficent. It is set to be the value of the highest allocated file 
 * descriptor. This saves having to search the entire pool of unallocated
 * file descriptors when looking for a match.
 */

alt_32 alt_max_fd = -1;

/*
 * "alt_fd_list" is the file descriptor pool. The first three entries in the
 * array are configured as standard in, standard out, and standard error. These
 * are all initialised so that accesses are directed to the alt_dev_null 
 * device. The remaining file descriptors are initialised as unallocated.
 *
 * The maximum number of file descriptors within the system is specified by the
 * user defined macro "ALT_MAX_FD". This is defined in "system.h", which is 
 * auto-genereated using the projects PTF and STF files.
 */

alt_fd alt_fd_list[ALT_MAX_FD] = 
  {
    {
      &alt_dev_null, /* standard in */
      0,
      0
    },
    {
      &alt_dev_null, /* standard out */
      0,
      0
    },
    {
      &alt_dev_null, /* standard error */
      0,
      0
    }
    /* all other elements are set to zero */
  };
