/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2004-2005 Altera Corporation, San Jose, California, USA.      *
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

off_t ALT_LSEEK (int file, off_t ptr, int dir)
{
  /* Generate a link time warning, should this function ever be called. */
  
  ALT_STUB_WARNING(lseek);
  
  /* Indicate an error */
  
  ALT_ERRNO = ENOSYS;
  return -1;
}

#else /* !ALT_USE_DIRECT_DRIVERS */

/*
 * lseek() can be called to move the read/write pointer associated with the 
 * file descriptor "file". This function simply vectors the call to the lseek()
 * function provided by the driver associated with the file descriptor.
 *
 * If the driver does not provide an implementation of lseek() an error is
 * indicated.
 *
 * lseek() corresponds to the standard lseek() function.
 *
 * ALT_LSEEK is mapped onto the lseek() system call in alt_syscall.h
 *
 */

off_t ALT_LSEEK (int file, off_t ptr, int dir)
{
  alt_fd* fd;
  off_t   rc = 0; 

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd) 
  {
    /*
     * If the device driver provides an implementation of the lseek() function,
     * then call that to process the request.
     */
 
    if (fd->dev->lseek)
    {
      rc = fd->dev->lseek(fd, ptr, dir);
    }
    /*
     * Otherwise return an error.
     */

    else
    {
      rc = -ENOTSUP;
    }
  }
  else  
  {
    rc = -EBADFD;
  }

  if (rc < 0)
  {
    ALT_ERRNO = -rc;
    rc = -1;
  }

  return rc;
}

#endif /* ALT_USE_DIRECT_DRIVERS */
