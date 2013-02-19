/*  alt_log_macro.S
 *
 *  Implements the function tx_log_str, called by the assembly macro 
 *  ALT_LOG_PUTS().  The macro will be empty when logging is turned off,
 *  and this function will not be compiled.  When logging is on,
 *  this function is used to print out the strings defined in the beginning
 *  of alt_log_printf.c, using port information taken from system.h and 
 *  alt_log_printf.h.  
 *
 *  This routine only handles strings, and sends a character into the defined
 *  output device's output buffer when the device is ready. It's intended for 
 *  debugging purposes, where messages can be set to print out at certain 
 *  points in the boot code to indicate the progress of the program.
 *
 */

#ifndef __ALT_LOG_MACROS__
#define __ALT_LOG_MACROS__

/* define this flag to skip assembly-incompatible parts
 * of various include files. */
#define ALT_ASM_SRC

#ifdef ALT_LOG_ENABLE // only compile this function if this flag is defined. 
	
	#include "system.h"
	#include "sys/alt_log_printf.h"

        .global tx_log_str
tx_log_str:
	/* load base uart / jtag uart address into r6 */
	movhi r6, %hiadj(ALT_LOG_PORT_BASE)
	addi r6, r6, %lo(ALT_LOG_PORT_BASE)
tx_next_char:
        /* if pointer points to null, return 
         * r4 is the pointer to the str to be printed, set by ALT_LOG_PUTS */
	ldb r7, (r4)
	beq r0, r7, end_tx

	/* check device transmit ready  */
wait_tx_ready_loop:
        ldwio   r5, ALT_LOG_PRINT_REG_OFFSET(r6)
        andi    r5, r5, ALT_LOG_PRINT_MSK
        beq     r5, r0, wait_tx_ready_loop	
	/* write char */
	stwio   r7, ALT_LOG_PRINT_TXDATA_REG_OFFSET (r6)		
	/* advance string pointer */
	addi r4, r4, 1
	br tx_next_char
end_tx:	
        ret
    
#endif

#endif /* __ALT_LOG_MACROS__ */

