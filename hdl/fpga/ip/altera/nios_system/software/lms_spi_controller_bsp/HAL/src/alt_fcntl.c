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
#include <fcntl.h>

#include <stdio.h>
#include <stdarg.h>

#include "sys/alt_errno.h"
#include "priv/alt_file.h"
#include "alt_types.h"
#include "os/alt_syscall.h"

#define ALT_FCNTL_FLAGS_MASK ((alt_u32) (O_APPEND | O_NONBLOCK))

/*
 * fcntl() is a limited implementation of the standard fcntl() system call.
 * It can be used to change the state of the flags associated with an open
 * file descriptor. Normally these flags are set during the call to
 * open(). It is anticipated that the main use of this function will be to
 * change the state of a device from blocking to non-blocking (where this is
 * supported). 
 *
 * The input argument "fd" is the file descriptor to be manipulated. "cmd"
 * is the command to execute. This can be either F_GETFL (return the 
 * current value of the flags) or F_SETFL (set the value of the flags).
 *
 * If "cmd" is F_SETFL then the argument "arg" is the new value of flags,
 * otherwise "arg" is ignored. Only the flags: O_APPEND and O_NONBLOCK
 * can be updated by a call to fcntl(). All other flags remain
 * unchanged.
 *
 * ALT_FCNTL is mapped onto the fcntl() system call in alt_syscall.h
 */
 
int ALT_FCNTL (int file, int cmd, ...)
{ 
  alt_fd*  fd;
  long     flags;
  va_list  argp;

  /*
   * A common error case is that when the file descriptor was created, the call
   * to open() failed resulting in a negative file descriptor. This is trapped
   * below so that we don't try and process an invalid file descriptor.
   */

  fd = (file < 0) ? NULL : &alt_fd_list[file];
  
  if (fd)
  {
    switch (cmd)
    {
    case F_GETFL:
      return fd->fd_flags & ~((alt_u32) ALT_FD_FLAGS_MASK);
    case F_SETFL:
      va_start(argp, cmd);
      flags = va_arg(argp, long);
      fd->fd_flags &= ~ALT_FCNTL_FLAGS_MASK;
      fd->fd_flags |= (flags & ALT_FCNTL_FLAGS_MASK);
      va_end(argp);
      return 0;
    default:
      ALT_ERRNO = EINVAL;
      return -1;
    }
  }

  ALT_ERRNO = EBADFD;
  return -1;
}
