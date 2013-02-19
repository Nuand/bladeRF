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

#include <signal.h>
#include <unistd.h>

#include "sys/alt_errno.h"
#include "os/alt_syscall.h"


/*
 * kill() is used by newlib in order to send signals to processes. Since there
 * is only a single process in the HAL, the only valid values for pid are 
 * either the current process id, or the broadcast values, i.e. pid must be
 * less than or equal to zero. 
 *
 * ALT_KILL is mapped onto the kill() system call in alt_syscall.h
 */

int ALT_KILL (int pid, int sig)
{
  int status = 0;

  if (pid <= 0)
  {
    switch (sig)
    {
    case 0:

      /* The null signal is used to check that a pid is valid. */

      break;

    case SIGABRT:
    case SIGALRM:
    case SIGFPE:
    case SIGILL:
    case SIGKILL:
    case SIGPIPE:
    case SIGQUIT:
    case SIGSEGV:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
    case SIGBUS:
    case SIGPOLL:
    case SIGPROF:
    case SIGSYS:
    case SIGTRAP:
    case SIGVTALRM:
    case SIGXCPU:
    case SIGXFSZ:

      /* 
       * The Posix standard defines the default behaviour for all these signals 
       * as being eqivalent to a call to _exit(). No mechanism is provided to 
       * change this behaviour.
       */

      _exit(0);
    case SIGCHLD:
    case SIGURG:

      /* 
       * The Posix standard defines these signals to be ignored by default. No 
       * mechanism is provided to change this behaviour.
       */

      break;
    default:

      /* Tried to send an unsupported signal */

      status = EINVAL;
    }
  }

  else if (pid > 0)
  {
    /* Attempted to signal a non-existant process */

    status = ESRCH;
  }

  if (status)
  {
    ALT_ERRNO = status;
    return -1;
  }

  return 0;
}
