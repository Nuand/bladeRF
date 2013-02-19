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
#include <string.h>

#include "sys/alt_dev.h"
#include "priv/alt_file.h"

#include "alt_types.h"

/* 
 * alt_find_file() is used by open() in order to locate a previously registered 
 * filesystem that owns that mount point that contains the file named "name". 
 *
 * The return value is a pointer to the matching filesystem, or NULL if there is
 * no match. 
 *
 * A match is considered to have been found if the filesystem name followed by
 * either '/' or '\0' is the prefix of the filename. For example the filename:
 * "/myfilesystem/junk.txt" would match: "/myfilesystem", but not: "/myfile". 
 */
 
alt_dev* alt_find_file (const char* name)
{
  alt_dev* next = (alt_dev*) alt_fs_list.next;   

  alt_32 len;
 
  /*
   * Check each list entry in turn, until a match is found, or we reach the
   * end of the list (i.e. next winds up pointing back to the list head).
   */ 
 
  while (next != (alt_dev*) &alt_fs_list)
  {
    len = strlen(next->name);
    
    if (next->name[len-1] == '/')
    {
      len -= 1;
    }

    if (((name[len] == '/') || (name[len] == '\0')) && 
        !memcmp (next->name, name, len))
    {
      /* match found */

      return next;
    }
    next = (alt_dev*) next->llist.next;
  }
  
  /* No match found */
  
  return NULL;     
}


