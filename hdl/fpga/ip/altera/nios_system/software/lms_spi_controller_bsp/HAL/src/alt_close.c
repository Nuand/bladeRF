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

#include <unistd.h>

#include "sys/alt_errno.h"
#include "sys/alt_warning.h"
#include "priv/alt_file.h"
#include "os/alt_syscall.h"

#ifdef ALT_USE_DIRECT_DRIVERS

int ALT_CLOSE (int fildes)
{
  /* Generate a link time warning, should this function ever be called. */
  
  ALT_STUB_WARNING(close);
  
  /* Indicate an error */
  
  ALT_ERRNO = ENOSYS;
  return -1;
}

#else /* !ALT_USE_DIRECT_DRIVERS */

/*
 * close() is called by an application to release a file descriptor. If the
 * associated file system/device has a close() callback function registered 
 * then this called. The file descriptor is then marked as free.
 *
 * ALT_CLOSE is mapped onto the close() system call in alt_syscall.h
 */
 
int ALT_CLOSE (int fildes)
{
  alt_fd* fd;
  int     rval;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (fildes < 0) ? NULL : &alt_fd_list[fildes];

  if (fd)
  {
    /*
     * If the associated file system/device has a close function, call it so 
     * that any necessary cleanup code can run.
     */

    rval = (fd->dev->close) ? fd->dev->close(fd) : 0;

    /* Free the file descriptor structure and return. */

    alt_release_fd (fildes);
    if (rval < 0)
    {
      ALT_ERRNO = -rval;
      return -1;
    }
    return 0;
  }
  else
  {
    ALT_ERRNO = EBADFD;
    return -1;
  }
}

#endif /* ALT_USE_DIRECT_DRIVERS */
