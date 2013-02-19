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

#include <unistd.h>
#include <fcntl.h>

#include "sys/alt_errno.h"
#include "sys/alt_warning.h"
#include "priv/alt_file.h"
#include "os/alt_syscall.h"

/*
 * The read() system call is used to read a block of data from a file or device.
 * This function simply vectors the request to the device driver associated
 * with the input file descriptor "file". 
 *
 * ALT_READ is mapped onto the read() system call in alt_syscall.h
 */

#ifdef ALT_USE_DIRECT_DRIVERS

#include "system.h"
#include "sys/alt_driver.h"

/*
 * Provide minimal version that just reads from the stdin device when provided.
 */

int ALT_READ (int file, void *ptr, size_t len)
{
#ifdef ALT_STDIN_PRESENT
    ALT_DRIVER_READ_EXTERNS(ALT_STDIN_DEV);
#endif

#if !defined(ALT_STDIN_PRESENT)
    /* Generate a link time warning, should this function ever be called. */
    ALT_STUB_WARNING(read);
#endif

    switch (file) {
#ifdef ALT_STDIN_PRESENT
    case 0: /* stdin file descriptor */
        return ALT_DRIVER_READ(ALT_STDIN_DEV, ptr, len, 0);
#endif /* ALT_STDIN_PRESENT */
    default:
        ALT_ERRNO = EBADFD;
        return -1;
    }
}

#else /* !ALT_USE_DIRECT_DRIVERS */

int ALT_READ (int file, void *ptr, size_t len)
{
  alt_fd*  fd;
  int      rval;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd)
  {
    /*
     * If the file has not been opened with read access, or if the driver does
     * not provide an implementation of read(), generate an error. Otherwise
     * call the drivers read() function to process the request.
     */

    if (((fd->fd_flags & O_ACCMODE) != O_WRONLY) && 
        (fd->dev->read))
      {
        if ((rval = fd->dev->read(fd, ptr, len)) < 0)
        {
          ALT_ERRNO = -rval;
          return -1;
        }
        return rval;
      }
      else
      {
        ALT_ERRNO = EACCES;
      }
    }
  else
  {
    ALT_ERRNO = EBADFD;
  }
  return -1;
}

#endif /* ALT_USE_DIRECT_DRIVERS */
