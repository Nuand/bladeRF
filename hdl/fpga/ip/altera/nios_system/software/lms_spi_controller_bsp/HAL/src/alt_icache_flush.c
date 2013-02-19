/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003 Altera Corporation, San Jose, California, USA.           *
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
******************************************************************************/

#include "nios2.h"
#include "system.h"

#include "alt_types.h"
#include "sys/alt_cache.h" 

/*
 * alt_icache_flush() is called to flush the instruction cache for a memory
 * region of length "len" bytes, starting at address "start".
 */

void alt_icache_flush (void* start, alt_u32 len)
{
#if NIOS2_ICACHE_SIZE > 0

  char* i;
  char* end;

  /*
   * This is the most we would ever need to flush.
   */
 
  if (len > NIOS2_ICACHE_SIZE)
  {
    len = NIOS2_ICACHE_SIZE;
  }

  end = ((char*) start) + len;

  for (i = start; i < end; i+= NIOS2_ICACHE_LINE_SIZE)
  { 
    __asm__ volatile ("flushi %0" :: "r" (i)); 
  }

  /* 
   * For an unaligned flush request, we've got one more line left.
   * Note that this is dependent on NIOS2_ICACHE_LINE_SIZE to be a 
   * multiple of 2 (which it always is).
   */

  if (((alt_u32) start) & (NIOS2_ICACHE_LINE_SIZE - 1))
  {
    __asm__ volatile ("flushi %0" :: "r" (i));
  }

  /* 
   * Having flushed the cache, flush any stale instructions in the 
   * pipeline 
   */

  __asm__ volatile ("flushp");

#endif /* NIOS2_ICACHE_SIZE > 0 */
}
