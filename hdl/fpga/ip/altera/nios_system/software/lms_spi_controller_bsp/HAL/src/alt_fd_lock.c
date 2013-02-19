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

#include <errno.h>

#include "priv/alt_file.h"

/*
 * alt_fd_lock() is called as a consequence of an ioctl call to gain exclusive
 * access to a device, i.e.:
 *
 * ioctl (fd, TIOCEXCL, NULL);
 *
 * If there are no other open file descriptors which reference the same
 * device, then alt_fd_lock() will grant the lock. Further calls to open() 
 * for this device will fail until the lock is released.
 *
 * This is done by calling close() for this file descriptor, or by calling:
 *
 * ioctl (fd, TIOCNXCL, NULL);
 *
 * The return value is zero for success, or negative in the case of failure.
 */

int alt_fd_lock (alt_fd* fd)
{
  int i;
  int rc = 0;

  ALT_SEM_PEND(alt_fd_list_lock, 0);

  for (i = 0; i < alt_max_fd; i++)
  {
    if ((&alt_fd_list[i] != fd) && (alt_fd_list[i].dev == fd->dev))
    {
      rc = -EACCES;
      goto alt_fd_lock_exit;
    }
  }
  fd->fd_flags |= ALT_FD_EXCL;

 alt_fd_lock_exit:

  ALT_SEM_POST(alt_fd_list_lock);
  return rc;
}
