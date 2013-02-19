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

#include "sys/ioctl.h"
#include "sys/alt_errno.h"
#include "sys/alt_warning.h"
#include "priv/alt_file.h"
#include "os/alt_syscall.h"

/*
 * The ioctl() system call is provided so that application code can manipulate 
 * the i/o capabilities of a device in device specific ways. This is identical
 * to the standard posix ioctl() function.
 *
 * In general this implementation simply vectors ioctl requests to the 
 * apropriate drivers ioctl function (as registered in the drivers alt_dev
 * structure).
 *
 * However in the case of devices (as oposed to filesystem), the TIOCEXCL and
 * TIOCNXCL requests are handled without reference to the driver. These 
 * requests are used to lock/release a device for exclusive access.
 *
 * Handling these requests centrally eases the task of device driver 
 * development. 
 *
 * ALT_IOCTL is mapped onto the ioctl() system call in alt_syscall.h 
 */
 
#ifdef ALT_USE_DIRECT_DRIVERS

#include "system.h"
#include "sys/alt_driver.h"

/*
 * Provide minimal version that calls ioctl routine of provided stdio devices.
 */
int ALT_IOCTL (int file, int req, void* arg)
{
#ifdef ALT_STDIN_PRESENT
    ALT_DRIVER_IOCTL_EXTERNS(ALT_STDIN_DEV);
#endif
#ifdef ALT_STDOUT_PRESENT
    ALT_DRIVER_IOCTL_EXTERNS(ALT_STDOUT_DEV);
#endif
#ifdef ALT_STDERR_PRESENT
    ALT_DRIVER_IOCTL_EXTERNS(ALT_STDERR_DEV);
#endif

#if !defined(ALT_STDIN_PRESENT) && !defined(ALT_STDOUT_PRESENT) && !defined(ALT_STDERR_PRESENT)
    /* Generate a link time warning, should this function ever be called. */
    ALT_STUB_WARNING(ioctl);
#endif

    switch (file) {
#ifdef ALT_STDIN_PRESENT
    case 0: /* stdin file descriptor */
        return ALT_DRIVER_IOCTL(ALT_STDIN_DEV, req, arg);
#endif /* ALT_STDIN_PRESENT */
#ifdef ALT_STDOUT_PRESENT
    case 1: /* stdout file descriptor */
        return ALT_DRIVER_IOCTL(ALT_STDOUT_DEV, req, arg);
#endif /* ALT_STDOUT_PRESENT */
#ifdef ALT_STDERR_PRESENT
    case 2: /* stderr file descriptor */
        return ALT_DRIVER_IOCTL(ALT_STDERR_DEV, req, arg);
#endif /* ALT_STDERR_PRESENT */
    default:
        ALT_ERRNO = EBADFD;
        return -1;
    }
}

#else /* !ALT_USE_DIRECT_DRIVERS */

int ALT_IOCTL (int file, int req, void* arg)
{
  alt_fd* fd;
  int     rc;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd)
  {

    /*
     * In the case of device drivers (not file systems) handle the TIOCEXCL
     * and TIOCNXCL requests as special cases.
     */

    if (fd->fd_flags & ALT_FD_DEV)
    {
      if (req == TIOCEXCL)
      {
        rc = alt_fd_lock (fd);
        goto ioctl_done;
      }
      else if (req == TIOCNXCL)
      {
        rc = alt_fd_unlock (fd);
        goto ioctl_done;
      }
    }

    /*
     * If the driver provides an ioctl() function, call that to handle the
     * request, otherwise set the return code to indicate that the request
     * could not be processed.
     */
   
    if (fd->dev->ioctl)
    {
      rc = fd->dev->ioctl(fd, req, arg);
    }
    else
    {
      rc = -ENOTTY;
    }
  }
  else  
  {
    rc = -EBADFD;
  }

ioctl_done:

  if (rc < 0)
  {
    ALT_ERRNO = -rc;
  }
  return rc;
}

#endif /* ALT_USE_DIRECT_DRIVERS */
