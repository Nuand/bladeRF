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

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "priv/nios2_gmon_data.h"

#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"


/* Macros */

/* How large should the bins be which we use to generate the histogram */
#define PCSAMPLE_BYTES_PER_BUCKET 32

#define NIOS2_READ_EA(dest)  __asm__ ("mov %0, ea" : "=r" (dest))

/* The compiler inserts calls to mcount() at the start of
 * every function call. The structure mcount_fn_arc records t
 * he return address of the function called (in from_pc)
 * and the return address of the mcount function
 * (in self_pc). The number of times this arc is executed is
 * recorded in the field count.
 */
struct mcount_fn_arc
{
  struct mcount_fn_arc * next;
  void * from_pc;
  unsigned int count;
};

/* We need to maintain a list of pointers to the heads of each adjacency
 * list so that we can find them when writing out the gmon.out file. Since
 * we don't know at the start of program execution how many functions will
 * be called we use a list structure to do this.
 */
struct mcount_fn_entry
{
  struct mcount_fn_entry * next;
  void * self_pc;
  struct mcount_fn_arc * arc_head;
};

/* function prototypes */

void __mcount_record(void * self_pc, void * from_pc, struct mcount_fn_entry * fn_entry, struct mcount_fn_entry * * fn_head) __attribute__ ((no_instrument_function));

static __inline__ void * mcount_allocate(unsigned int size) __attribute__ ((no_instrument_function));
static int nios2_pcsample_init(void) __attribute__ ((no_instrument_function));
static alt_u32 nios2_pcsample(void* alarm) __attribute__ ((no_instrument_function));

/* global variables */

/* stext and etext are defined in the linker script */
extern char stext[];
extern char etext[];

/* Is the PC sampling stuff enabled yet? */
static int pcsample_need_init = 1;

#define HASH_BUCKETS 64 /* Must be a power of 2 */

/* This points to the list of adjacency list pointers. */
struct mcount_fn_entry * __mcount_fn_head[HASH_BUCKETS];

/* pointer to the in-memory buffer containing the histogram */
static unsigned short* s_pcsamples = 0;

/* the address of the start and end of text section */
static const unsigned int s_low_pc  = (unsigned int)stext;
static const unsigned int s_high_pc = (unsigned int)etext;

/* the alarm structure to register for pc sampling */
static alt_alarm s_nios2_pcsample_alarm;

unsigned int alt_gmon_data[GMON_DATA_SIZE] =
{
  0x6e6f6d67, /* "gmon" */
  GMON_DATA_SIZE,
  0,
  (unsigned int)stext,
  (unsigned int)etext,
  PCSAMPLE_BYTES_PER_BUCKET,
  0,
  (unsigned int)__mcount_fn_head,
  (unsigned int)(__mcount_fn_head + HASH_BUCKETS)
};

/* This holds the current slab of memory we're allocating out of */
static char * mcount_slab_ptr = 0;
static int    mcount_slab_size = 0;

#define MCOUNT_SLAB_INCREMENT 1020


/*
 * We can't use malloc to allocate memory because that's too complicated, and
 * can't be called at interrupt time.  Use the lower level allocator instead
 * because that's interrupt safe (and because we never free anything).
 *
 * For speed, we allocate a block of data at once.
 */
static __inline__ void * mcount_allocate(unsigned int size)
{
  void * data;

  if (size > mcount_slab_size)
  {
    mcount_slab_ptr = sbrk(MCOUNT_SLAB_INCREMENT);
    mcount_slab_size = MCOUNT_SLAB_INCREMENT;
  }

  data = mcount_slab_ptr;
  mcount_slab_ptr += size;
  mcount_slab_size -= size;

  return data;
}


/*
 * Add the arc with the values of frompc and topc given to the graph.
 * This function might be called at interrupt time so must be able to
 * cope with reentrancy.
 *
 * The fast case, where we have already allocated a function arc, has been
 * handled by the assmebler code.
 */
void __mcount_record(void * self_pc, void * from_pc, struct mcount_fn_entry * fn_entry, struct mcount_fn_entry * * fn_head)
{
  alt_irq_context context;
  struct mcount_fn_arc * arc_entry;

  /* Keep trying to start up the PC sampler until it is running.
   * (It can't start until the timer is going).
   */
  if (pcsample_need_init)
  {
    pcsample_need_init = 0;
    pcsample_need_init = nios2_pcsample_init();
  }

  /*
   * We must disable interrupts around the allocation and the list update to
   * prevent corruption if the instrumented function is re-entrant.
   *
   * It's safe for the code above to be stepping through the chain and be
   * interrupted by this code modifying it - there is an edge case which will
   * leave two copies of the same arc on the list (both with count=1), but
   * this is dealt with on the host.
   */
  context = alt_irq_disable_all();

  if (fn_entry == NULL)
  {
    /* Add it to the list of functions we must output later. */
    fn_entry = (struct mcount_fn_entry *)mcount_allocate(sizeof(struct mcount_fn_entry));

    fn_entry->self_pc = self_pc;
    fn_entry->arc_head = NULL;

    fn_entry->next = *fn_head;
    *fn_head = fn_entry;
  }

  /* We will need a new list entry - if there was a list entry before
   * then the assembler code would have handled it. */
  arc_entry = (struct mcount_fn_arc *)mcount_allocate(sizeof(struct mcount_fn_arc));

  arc_entry->from_pc = from_pc;
  arc_entry->count = 1;

  arc_entry->next = fn_entry->arc_head;
  fn_entry->arc_head = arc_entry;

  alt_irq_enable_all(context);
}


/*
 * nios2_pcsample_init starts profiling.
 * It is called the first time mcount is called, and on subsequent calls to
 * mcount until it returns zero. It initializes the pc histogram and turns on
 * timer driven pc sampling.
 */
static int nios2_pcsample_init(void)
{
  unsigned int pcsamples_size; 

  /* We sample the PC every tick */
  unsigned int prof_rate = alt_ticks_per_second();
  if (prof_rate == 0)
    return 1;

  /* allocate the histogram buffer s_pcsamples */
  pcsamples_size = (s_high_pc - s_low_pc)/PCSAMPLE_BYTES_PER_BUCKET;
  s_pcsamples    = (unsigned short*)sbrk(pcsamples_size * sizeof(unsigned short));

  if (s_pcsamples != 0)
  {
    /* initialize the buffer to zero */
    memset(s_pcsamples, 0, pcsamples_size * sizeof(unsigned short));

    alt_gmon_data[GMON_DATA_PROFILE_DATA] = (int)s_pcsamples;
    alt_gmon_data[GMON_DATA_PROFILE_RATE] = prof_rate;

    /* Sample every tick (it's cheap) */
    alt_alarm_start(&s_nios2_pcsample_alarm, 1, nios2_pcsample, 0);
  }

  return 0;
}


/*
 * Sample the PC value and store it in the histogram
 */
static alt_u32 nios2_pcsample(void* context)
{
  unsigned int pc;
  unsigned int bucket;

  /* read the exception return address - this will be
   * inaccurate if there are nested interrupts but we
   * assume that this is rare and the inaccuracy will
   * not be great */
  NIOS2_READ_EA(pc);

  /*
   * If we're within the profilable range then increment the relevant
   * bucket in the histogram
   */
  if (pc >= s_low_pc && pc < s_high_pc && s_pcsamples != 0)
  {
    bucket = (pc - s_low_pc)/PCSAMPLE_BYTES_PER_BUCKET;
    s_pcsamples[bucket]++;
  }

  /* Sample every tick */
  return 1;
}

