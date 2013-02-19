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

#include <stddef.h>
#include <sys/stat.h>

#include "sys/alt_dev.h"
#include "sys/alt_warning.h"
#include "sys/alt_errno.h"
#include "priv/alt_file.h"
#include "os/alt_syscall.h"

/*
 * The fstat() system call is used to obtain information about the capabilities
 * of an open file descriptor. By default file descriptors are marked as 
 * being character devices. If a device or file system wishes to advertise 
 * alternative capabilities then they can register an fstat() function within
 * their associated alt_dev structure. This will be called to fill in the
 * entries in the imput "st" structure.
 *
 * This function is provided for compatability with newlib. 
 *
 * ALT_FSTAT is mapped onto the fstat() system call in alt_syscall.h
 */

#ifdef ALT_USE_DIRECT_DRIVERS

#include "system.h"

/*
 * Provide minimal version that just describes all file descriptors 
 * as character devices for provided stdio devices.
 */
int ALT_FSTAT (int file, struct stat *st)
{
    switch (file) {
#ifdef ALT_STDIN_PRESENT
    case 0: /* stdin file descriptor */
#endif /* ALT_STDIN_PRESENT */
#ifdef ALT_STDOUT_PRESENT
    case 1: /* stdout file descriptor */
#endif /* ALT_STDOUT_PRESENT */
#ifdef ALT_STDERR_PRESENT
    case 2: /* stderr file descriptor */
#endif /* ALT_STDERR_PRESENT */
        st->st_mode = _IFCHR;
        return 0;
    default:
        return -1;
    }

#if !defined(ALT_STDIN_PRESENT) && !defined(ALT_STDOUT_PRESENT) && !defined(ALT_STDERR_PRESENT)
    /* Generate a link time warning, should this function ever be called. */
    ALT_STUB_WARNING(fstat);
#endif
}

#else /* !ALT_USE_DIRECT_DRIVERS */

int ALT_FSTAT (int file, struct stat *st)
{
  alt_fd*  fd;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd)
  {
    /* Call the drivers fstat() function to fill out the "st" structure. */

    if (fd->dev->fstat)
    {
      return fd->dev->fstat(fd, st);
    }

    /* 
     * If no function is provided, mark the fd as belonging to a character 
     * device.
     */
 
    else
    {
      st->st_mode = _IFCHR;
      return 0;
    }
  }
  else
  {
    ALT_ERRNO = EBADFD;
    return -1;
  }
}

#endif /* ALT_USE_DIRECT_DRIVERS */
