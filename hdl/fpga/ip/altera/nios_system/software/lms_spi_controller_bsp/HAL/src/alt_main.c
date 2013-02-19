/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2007 Altera Corporation, San Jose, California, USA.           *
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "sys/alt_dev.h"
#include "sys/alt_sys_init.h"
#include "sys/alt_irq.h"
#include "sys/alt_dev.h"

#include "os/alt_hooks.h"

#include "priv/alt_file.h"
#include "alt_types.h"

#include "system.h"

#include "sys/alt_log_printf.h"

extern void _do_ctors(void);
extern void _do_dtors(void);

/*
 * Standard arguments for main. By default, no arguments are passed to main.
 * However a device driver may choose to configure these arguments by calling
 * alt_set_args(). The expectation is that this facility will only be used by
 * the iclient/ihost utility.
 */

int    alt_argc = 0;
char** alt_argv = {NULL};
char** alt_envp = {NULL};

/*
 * Prototype for the entry point to the users application.
 */

extern int main (int, char **, char **);

/*
 * alt_main is the C entry point for the HAL. It is called by the assembler
 * startup code in the processor specific crt0.S. It is responsible for:
 * completing the C runtime configuration; configuring all the
 * devices/filesystems/components in the system; and call the entry point for
 * the users application, i.e. main().
 */

void alt_main (void)
{
#ifndef ALT_NO_EXIT    
  int result;
#endif

  /* ALT LOG - please see HAL/sys/alt_log_printf.h for details */
  ALT_LOG_PRINT_BOOT("[alt_main.c] Entering alt_main, calling alt_irq_init.\r\n");
  /* Initialize the interrupt controller. */
  alt_irq_init (NULL);

  /* Initialize the operating system */
  ALT_LOG_PRINT_BOOT("[alt_main.c] Done alt_irq_init, calling alt_os_init.\r\n");
  ALT_OS_INIT();

  /*
   * Initialize the semaphore used to control access to the file descriptor
   * list.
   */

  ALT_LOG_PRINT_BOOT("[alt_main.c] Done OS Init, calling alt_sem_create.\r\n");
  ALT_SEM_CREATE (&alt_fd_list_lock, 1);

  /* Initialize the device drivers/software components. */
  ALT_LOG_PRINT_BOOT("[alt_main.c] Calling alt_sys_init.\r\n");
  alt_sys_init();
  ALT_LOG_PRINT_BOOT("[alt_main.c] Done alt_sys_init.\r\n");

#if !defined(ALT_USE_DIRECT_DRIVERS) && (defined(ALT_STDIN_PRESENT) || defined(ALT_STDOUT_PRESENT) || defined(ALT_STDERR_PRESENT))

  /*
   * Redirect stdio to the apropriate devices now that the devices have
   * been initialized. This is only done if the user has requested these
   * devices be present (not equal to /dev/null) and if direct drivers
   * aren't being used.
   */

    ALT_LOG_PRINT_BOOT("[alt_main.c] Redirecting IO.\r\n");
    alt_io_redirect(ALT_STDOUT, ALT_STDIN, ALT_STDERR);
#endif

#ifndef ALT_NO_C_PLUS_PLUS
  /* 
   * Call the C++ constructors 
   */

  ALT_LOG_PRINT_BOOT("[alt_main.c] Calling C++ constructors.\r\n");
  _do_ctors ();
#endif /* ALT_NO_C_PLUS_PLUS */

#if !defined(ALT_NO_C_PLUS_PLUS) && !defined(ALT_NO_CLEAN_EXIT) && !defined(ALT_NO_EXIT)
  /*
   * Set the C++ destructors to be called at system shutdown. This is only done
   * if a clean exit has been requested (i.e. the exit() function has not been
   * redefined as _exit()). This is in the interest of reducing code footprint,
   * in that the atexit() overhead is removed when it's not needed.
   */

  ALT_LOG_PRINT_BOOT("[alt_main.c] Calling atexit.\r\n");
  atexit (_do_dtors);
#endif

  /*
   * Finally, call main(). The return code is then passed to a subsequent
   * call to exit() unless the application is never supposed to exit.
   */

  ALT_LOG_PRINT_BOOT("[alt_main.c] Calling main.\r\n");

#ifdef ALT_NO_EXIT
  main (alt_argc, alt_argv, alt_envp);
#else
  result = main (alt_argc, alt_argv, alt_envp);
  close(STDOUT_FILENO);
  exit (result);
#endif

  ALT_LOG_PRINT_BOOT("[alt_main.c] After main - we should not be here?.\r\n");
}

