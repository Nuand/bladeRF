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

#include "sys/alt_dev.h"
#include "priv/alt_file.h"

/*
 * The alt_fs_reg() function is used to register a file system. Once registered 
 * a device can be accessed using the standard posix calls: open(), read(), 
 * write() etc.
 *
 * System behaviour is undefined in the event that a file system is registered 
 * with a name that conflicts with an existing device or file system.
 *
 * alt_fs_reg() is not thread safe in the sense that there should be no other 
 * thread using the file system list at the time that alt_dev_reg() is called. In
 * practice this means that alt_fs_reg() should only be called while operating
 * in a single threaded mode. The expectation is that it will only be called
 * by the file system initilisation functions invoked by alt_sys_init(), which in 
 * turn should only be called by the single threaded C startup code.   
 *
 * A return value of zero indicates success. A negative return value indicates
 * failure. 
 */
 
int alt_fs_reg (alt_dev* dev)
{
  /*
   * check that the device has a name.
   */

  if (!dev->name)
  {
    return -ENODEV;
  }
  
  /*
   * register the file system.
   */

  alt_llist_insert(&alt_fs_list, &dev->llist);

  return 0;
} 
