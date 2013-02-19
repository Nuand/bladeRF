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

#include "sys/alt_errno.h"
#include "sys/alt_warning.h"
#include "priv/alt_file.h"
#include "alt_types.h"
#include "os/alt_syscall.h"

#ifdef ALT_USE_DIRECT_DRIVERS

int ALT_OPEN (const char* file, int flags, int mode)
{
  /* Generate a link time warning, should this function ever be called. */
  
  ALT_STUB_WARNING(open);
  
  /* Indicate an error */
  
  ALT_ERRNO = ENOSYS;
  return -1;
}

#else /* !ALT_USE_DIRECT_DRIVERS */

extern alt_llist alt_dev_list;

/*
 * alt_file_locked() is used by open() to ensure that a device has not been
 * previously locked for exclusive access using ioctl(). This test is only
 * performed for devices. Filesystems are required to handle the ioctl() call
 * themselves, and report the error from the filesystems open() function. 
 */ 

static int alt_file_locked (alt_fd* fd)
{
  alt_u32 i;

  /*
   * Mark the file descriptor as belonging to a device.
   */

  fd->fd_flags |= ALT_FD_DEV;

  /*
   * Loop through all current file descriptors searching for one that's locked
   * for exclusive access. If a match is found, generate an error.
   */

  for (i = 0; i <= alt_max_fd; i++)
  {
    if ((alt_fd_list[i].dev == fd->dev) &&
        (alt_fd_list[i].fd_flags & ALT_FD_EXCL) &&
        (&alt_fd_list[i] != fd))
    {
      return -EACCES;
    }
  }
  
  /* The device is not locked */
 
  return 0;
}

/*
 * open() is called in order to get a file descriptor that reference the file
 * or device named "name". This descriptor can then be used to manipulate the
 * file/device using the standard system calls, e.g. write(), read(), ioctl()
 * etc.
 *
 * This is equivalent to the standard open() system call.
 *
 * ALT_OPEN is mapped onto the open() system call in alt_syscall.h
 */
 
int ALT_OPEN (const char* file, int flags, int mode)
{ 
  alt_dev* dev;
  alt_fd*  fd;
  int index  = -1;
  int status = -ENODEV;
  int isafs = 0;

  /* 
   * Check the device list, to see if a device with a matching name is 
   * registered.
   */
  
  if (!(dev = alt_find_dev (file, &alt_dev_list)))
  {
    /* No matching device, so try the filesystem list */

    dev   = alt_find_file (file);
    isafs = 1;
  }

  /* 
   * If a matching device or filesystem is found, allocate a file descriptor. 
   */

  if (dev)
  {
    if ((index = alt_get_fd (dev)) < 0)
    {
      status = index;
    }
    else
    {
      fd = &alt_fd_list[index];
      fd->fd_flags = (flags & ~ALT_FD_FLAGS_MASK);
      
      /* If this is a device, ensure it isn't already locked */

      if (isafs || ((status = alt_file_locked (fd)) >= 0))
      {
        /* 
         * If the device or filesystem provides an open() callback function,
         * call it now to perform any device/filesystem specific operations.
         */
    
        status = (dev->open) ? dev->open(fd, file, flags, mode): 0;
      }
    }
  }
  else
  {
    status = -ENODEV;
  }

  /* Allocation failed, so clean up and return an error */ 

  if (status < 0)
  {
    alt_release_fd (index);  
    ALT_ERRNO = -status;
    return -1;
  }
  
  /* return the reference upon success */

  return index;
}

#endif /* ALT_USE_DIRECT_DRIVERS */
