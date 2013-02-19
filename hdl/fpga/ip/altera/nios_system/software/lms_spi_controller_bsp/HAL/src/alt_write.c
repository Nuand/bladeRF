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

#include "sys/alt_log_printf.h"

/*
 * The write() system call is used to write a block of data to a file or 
 * device. This function simply vectors the request to the device driver 
 * associated with the input file descriptor "file". 
 *
 * ALT_WRITE is mapped onto the write() system call in alt_syscall.h
 */
 
#ifdef ALT_USE_DIRECT_DRIVERS

#include "system.h"
#include "sys/alt_driver.h"

/*
 * Provide minimal version that just writes to the stdout/stderr devices
 * when provided.
 */

int ALT_WRITE (int file, const void *ptr, size_t len)
{
#ifdef ALT_STDOUT_PRESENT
    ALT_DRIVER_WRITE_EXTERNS(ALT_STDOUT_DEV);
#endif
#ifdef ALT_STDERR_PRESENT
    ALT_DRIVER_WRITE_EXTERNS(ALT_STDERR_DEV);
#endif

#if !defined(ALT_STDOUT_PRESENT) && !defined(ALT_STDERR_PRESENT)
    /* Generate a link time warning, should this function ever be called. */
    ALT_STUB_WARNING(write);
#endif

    switch (file) {
#ifdef ALT_STDOUT_PRESENT
    case 1: /* stdout file descriptor */
        return ALT_DRIVER_WRITE(ALT_STDOUT_DEV, ptr, len, 0);
#endif /* ALT_STDOUT_PRESENT */
#ifdef ALT_STDERR_PRESENT
    case 2: /* stderr file descriptor */
        return ALT_DRIVER_WRITE(ALT_STDERR_DEV, ptr, len, 0);
#endif /* ALT_STDERR_PRESENT */
    default:
        ALT_ERRNO = EBADFD;
        return -1;
    }
}

#else /* !ALT_USE_DIRECT_DRIVERS */

int ALT_WRITE (int file, const void *ptr, size_t len)
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
     * If the file has not been opened with write access, or if the driver does
     * not provide an implementation of write(), generate an error. Otherwise
     * call the drivers write() function to process the request.
     */

    if (((fd->fd_flags & O_ACCMODE) != O_RDONLY) && fd->dev->write)
    {
      
      /* ALT_LOG - see altera_hal/HAL/inc/sys/alt_log_printf.h */
      ALT_LOG_WRITE_FUNCTION(ptr,len);

      if ((rval = fd->dev->write(fd, ptr, len)) < 0)
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
