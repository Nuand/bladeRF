/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003-2005 Altera Corporation, San Jose, California, USA.      *
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
 * Nios II version 1.2 and newer supports the "flush by address" instruction, in
 * addition to the "flush by line" instruction provided by older versions of
 * the core. This newer instruction is used by preference when it is 
 * available.
 */

#ifdef NIOS2_FLUSHDA_SUPPORTED
#define ALT_FLUSH_DATA(i) __asm__ volatile ("flushda (%0)" :: "r" (i));
#else
#define ALT_FLUSH_DATA(i) __asm__ volatile ("flushd (%0)" :: "r" (i));
#endif /* NIOS2_FLUSHDA_SUPPORTED */

/*
 * alt_dcache_flush() is called to flush the data cache for a memory
 * region of length "len" bytes, starting at address "start".
 *
 * Any dirty lines in the data cache are written back to memory.
 */

void alt_dcache_flush (void* start, alt_u32 len)
{
#if NIOS2_DCACHE_SIZE > 0

  char* i;
  char* end; 

  /*
   * This is the most we would ever need to flush.
   *
   * SPR 196942, 2006.01.13: The cache flush loop below will use the
   * 'flushda' instruction if its available; in that case each line
   * must be flushed individually, and thus 'len' cannot be trimmed.
   */
  #ifndef NIOS2_FLUSHDA_SUPPORTED
  if (len > NIOS2_DCACHE_SIZE)
  {
    len = NIOS2_DCACHE_SIZE;
  }
  #endif

  end = ((char*) start) + len; 

  for (i = start; i < end; i+= NIOS2_DCACHE_LINE_SIZE)
  { 
    ALT_FLUSH_DATA(i); 
  }

  /* 
   * For an unaligned flush request, we've got one more line left.
   * Note that this is dependent on NIOS2_DCACHE_LINE_SIZE to be a 
   * multiple of 2 (which it always is).
   */

  if (((alt_u32) start) & (NIOS2_DCACHE_LINE_SIZE - 1))
  {
    ALT_FLUSH_DATA(i);
  }

#endif /* NIOS2_DCACHE_SIZE > 0 */
}
