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
#include <errno.h>

#include "sys/alt_dev.h"
#include "priv/alt_file.h"

#include "alt_types.h"

#include "system.h"

/*
 * alt_get_fd() is called to allocate a new file descriptor from the file
 * descriptor pool. If a file descriptor is succesfully allocated, it is 
 * configured to refer to device "dev".
 *
 * The return value is the index of the file descriptor structure (i.e. 
 * the offset of the file descriptor within the file descriptor array). A
 * negative value indicates failure.
 */

int alt_get_fd (alt_dev* dev)
{
  alt_32 i;
  int rc = -EMFILE;
  
  /* 
   * Take the alt_fd_list_lock semaphore in order to avoid races when 
   * accessing the file descriptor pool.
   */
  
  ALT_SEM_PEND(alt_fd_list_lock, 0);
  
  /* 
   * Search through the list of file descriptors, and allocate the first
   * free descriptor that's found. 
   *
   * If a free descriptor is found, then the value of "alt_max_fd" is 
   * updated accordingly. "alt_max_fd" is a 'highwater mark' which 
   * indicates the highest file descriptor ever allocated. This is used to
   * improve efficency when searching the file descriptor list, and 
   * therefore reduce contention on the alt_fd_list_lock semaphore. 
   */

  for (i = 0; i < ALT_MAX_FD; i++)
  {
    if (!alt_fd_list[i].dev)
    {
      alt_fd_list[i].dev = dev;
      if (i > alt_max_fd)
      {
        alt_max_fd = i;
      }
      rc = i;
      goto alt_get_fd_exit;
    }
  }

 alt_get_fd_exit:

  /*
   * Release the alt_fd_list_lock semaphore now that we are done with the
   * file descriptor pool.
   */

  ALT_SEM_POST(alt_fd_list_lock);

  return rc;
}




