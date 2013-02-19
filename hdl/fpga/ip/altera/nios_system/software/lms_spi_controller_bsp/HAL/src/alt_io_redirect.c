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

#include "sys/alt_dev.h"
#include "priv/alt_file.h"


/*
 * alt_open_fd() is similar to open() in that it is used to obtain a file
 * descriptor for the file named "name". The "flags" and "mode" arguments are
 * identical to the "flags" and "mode" arguments of open().
 *
 * The distinction between the two functions is that the file descriptor 
 * structure to use is passed in as an argument, rather than allocated from the
 * list of free file descriptors.
 *
 * This is used by alt_io_redirect() to redirect the stdin, stdout and stderr
 * file descriptors to point to new devices.
 *
 * If the device can not be succesfully opened, then the input file descriptor
 * remains unchanged.
 */

static void alt_open_fd(alt_fd* fd, const char* name, int flags, int mode)
{
  int old;

  old = open (name, flags, mode);

  if (old >= 0)
  {
    fd->dev      = alt_fd_list[old].dev;
    fd->priv     = alt_fd_list[old].priv;
    fd->fd_flags = alt_fd_list[old].fd_flags;

    alt_release_fd (old);
  }
} 

/*
 * alt_io_redirect() is called once the device/filesystem lists have been 
 * initialised, but before main(). Its function is to redirect standard in,
 * standard out and standard error so that they point to the devices selected by
 * the user (as defined in system.h).
 *
 * Prior to the call to this function, io is directed towards /dev/null. If
 * i/o can not be redirected to the requested device, for example if the device 
 * does not exist, then it remains directed at /dev/null. 
 */
 
void alt_io_redirect(const char* stdout_dev, 
                     const char* stdin_dev, 
                     const char* stderr_dev)
{
  /* Redirect the channels */

  alt_open_fd (&alt_fd_list[STDOUT_FILENO], stdout_dev, O_WRONLY, 0777);
  alt_open_fd (&alt_fd_list[STDIN_FILENO], stdin_dev, O_RDONLY, 0777);
  alt_open_fd (&alt_fd_list[STDERR_FILENO], stderr_dev, O_WRONLY, 0777);
}  




