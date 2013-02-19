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
#include "sys/alt_errno.h"
#include "sys/alt_warning.h"
#include "priv/alt_file.h"
#include "os/alt_syscall.h"

#ifdef ALT_USE_DIRECT_DRIVERS

#include "system.h"

/*
 * Provide minimal version that just describes all file descriptors 
 * as tty devices for provided stdio devices.
 */
int ALT_ISATTY (int file)
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
        return 1;
    default:
        return 0;
    }

#if !defined(ALT_STDIN_PRESENT) && !defined(ALT_STDOUT_PRESENT) && !defined(ALT_STDERR_PRESENT)
    /* Generate a link time warning, should this function ever be called. */
    ALT_STUB_WARNING(isatty);
#endif
}

#else /* !ALT_USE_DIRECT_DRIVERS */
/*
 * isatty() can be used to determine whether the input file descriptor "file" 
 * refers to a terminal device or not. If it is a terminal device then the
 * return value is one, otherwise it is zero.  
 *
 * ALT_ISATTY is mapped onto the isatty() system call in alt_syscall.h
 */
 
int ALT_ISATTY (int file)
{
  alt_fd*     fd;
  struct stat stat;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd)
  {
    /*
     * If a device driver does not provide an fstat() function, then it is 
     * treated as a terminal device by default.
     */

    if (!fd->dev->fstat)
    {
      return 1;
    }

    /*
     * If a driver does provide an implementation of the fstat() function, then
     * this is called so that the device can identify itself.
     */ 

    else
    {
      fstat (file, &stat);
      return (stat.st_mode == _IFCHR) ? 1 : 0;
    }
  }
  else
  {
    ALT_ERRNO = EBADFD;
    return 0;
  }
}

#endif /* ALT_USE_DIRECT_DRIVERS */
