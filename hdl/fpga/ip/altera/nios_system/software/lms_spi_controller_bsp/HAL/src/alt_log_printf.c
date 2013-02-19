
/* alt_log_printf.c
 *
 * This file implements the various C functions used for the 
 * alt_log logging/debugging print functions.  The functions
 * sit as is here - the job of hiding them from the compiler
 * if logging is disabled is accomplished in the .h file. 
 *
 * All the global variables for alt_log are defined here.  
 * These include the various flags that turn on additional 
 * logging options; the strings for assembly printing; and
 * other globals needed by different logging options. 
 *
 * There are 4 functions that handle the actual printing: 
 * alt_log_txchar: Actual function that puts 1 char to UART/JTAG UART.
 * alt_log_repchar: Calls alt_log_txchar 'n' times - used by
 *            alt_log_private_printf for formatting.
 * alt_log_private_printf:
 *     Stripped down implementation of printf - no floats.
 * alt_log_printf_proc:
 *     Wrapper function for private_printf.
 * 
 * The rest of the functions are called by the macros which
 * were called by code in the other components.  Each function
 * is preceded by a comment, about which file it gets called
 * in, and what its purpose is.
 *
 * author: gkwan
 */

/* skip all code if enable is off */
#ifdef ALT_LOG_ENABLE

#include <system.h>
#include <stdarg.h>
#include <string.h>
#ifdef __ALTERA_AVALON_JTAG_UART
   #include "altera_avalon_jtag_uart.h"
   #include <altera_avalon_jtag_uart_regs.h>
#endif
#include "sys/alt_log_printf.h"

/* strings for assembly puts */
char alt_log_msg_bss[] = "[crt0.S] Clearing BSS \r\n";;
char alt_log_msg_alt_main[] = "[crt0.S] Calling alt_main.\r\n";
char alt_log_msg_stackpointer[] \
    = "[crt0.S] Setting up stack and global pointers.\r\n";
char alt_log_msg_cache[] = "[crt0.S] Inst & Data Cache Initialized.\r\n";
/* char array allocation for alt_write */
char alt_log_write_buf[ALT_LOG_WRITE_ECHO_LEN+2];

/* global variables for all 'on' flags */

/* 
 * The boot message flag is linked into the data (rwdata) section
 * because if it is zero, it would otherwise be placed in the bss section.
 * alt_log examines this variable before the BSS is cleared in the boot-up
 * process.
 */
volatile alt_u32 alt_log_boot_on_flag \
  __attribute__ ((section (".data"))) = ALT_LOG_BOOT_ON_FLAG_SETTING;

volatile alt_u8 alt_log_write_on_flag = ALT_LOG_WRITE_ON_FLAG_SETTING;

volatile alt_u8 alt_log_sys_clk_on_flag = ALT_LOG_SYS_CLK_ON_FLAG_SETTING;

volatile alt_u8 alt_log_jtag_uart_alarm_on_flag = \
  ALT_LOG_JTAG_UART_ALARM_ON_FLAG_SETTING;

volatile alt_u8 alt_log_jtag_uart_isr_on_flag = \
  ALT_LOG_JTAG_UART_ISR_ON_FLAG_SETTING;

volatile alt_u8 alt_log_jtag_uart_startup_info_on_flag = \
  ALT_LOG_JTAG_UART_STARTUP_INFO_ON_FLAG_SETTING;

/* Global alarm object for recurrent JTAG UART status printing */
alt_alarm alt_log_jtag_uart_alarm_1;

/* Global ints for system clock printing and count */
volatile int alt_log_sys_clk_count;
volatile int alt_system_clock_in_sec;

/* enum used by alt_log_private_printf */
enum
{
  pfState_chars,
  pfState_firstFmtChar,
  pfState_otherFmtChar
};




/* Function to put one char onto the UART/JTAG UART txdata register. */
void alt_log_txchar(int c,char *base)
{
  /* Wait until the device is ready for a character */
  while((ALT_LOG_PRINT_REG_RD(base) & ALT_LOG_PRINT_MSK) == 0)
    ;
  /* And pop the character into the register */
  ALT_LOG_PRINT_TXDATA_WR(base,c);
}


/* Called by alt_log_private_printf to print out characters repeatedly */
void alt_log_repchar(char c,int r,int base)
{
  while(r-- > 0)
    alt_log_txchar(c,(char*) base);
}


/* Stripped down printf function */
void alt_log_private_printf(const char *fmt,int base,va_list args)
  {
  const char *w;
  char c;
  int state;
  int fmtLeadingZero = 0; /* init these all to 0 for -W warnings. */
  int fmtLong = 0;
  int fmtBeforeDecimal = 0;
  int fmtAfterDecimal = 0;
  int fmtBase = 0;
  int fmtSigned = 0;
  int fmtCase = 0; /* For hex format, if 1, A-F, else a-f. */

  w = fmt;
  state = pfState_chars;

  while(0 != (c = *w++))
    {
    switch(state)
      {
      case pfState_chars:
        if(c == '%')
        {
          fmtLeadingZero = 0;
          fmtLong = 0;
          fmtBase = 10;
          fmtSigned = 1;
          fmtCase = 0; /* Only %X sets this. */
          fmtBeforeDecimal = -1;
          fmtAfterDecimal = -1;
          state = pfState_firstFmtChar;
        }
        else
        {
          alt_log_txchar(c,(char*)base);
        }
        break;

      case pfState_firstFmtChar:
        if(c == '0')
        {
          fmtLeadingZero = 1;
          state = pfState_otherFmtChar;
        }
        else if(c == '%')
        {
          alt_log_txchar(c,(char*)base);
          state = pfState_chars;
        }
        else
        {
          state = pfState_otherFmtChar;
          goto otherFmtChar;
        }
        break;

      case pfState_otherFmtChar:
otherFmtChar:
        if(c == '.')
        {
          fmtAfterDecimal = 0;
        }
        else if('0' <= c && c <= '9')
        {
          c -= '0';
          if(fmtAfterDecimal < 0)     /* still before decimal */
          {
            if(fmtBeforeDecimal < 0)
            {
              fmtBeforeDecimal = 0;
            }
            else
            {
              fmtBeforeDecimal *= 10;
            }
            fmtBeforeDecimal += c;
          }
          else
          {
            fmtAfterDecimal = (fmtAfterDecimal * 10) + c;
          }
        }
        else if(c == 'l')
        {
          fmtLong = 1;
        }
        else                  /* we're up to the letter which determines type */
        {
          switch(c)
          {
            case 'd':
            case 'i':
doIntegerPrint:
                {
                unsigned long v;
                unsigned long p;  /* biggest power of fmtBase */
                unsigned long vShrink;  /* used to count digits */
                int sign;
                int digitCount;

                /* Get the value */
                if(fmtLong)
                {
                  if (fmtSigned)
                  {
                    v = va_arg(args,long);
                  }
                  else
                  {
                    v = va_arg(args,unsigned long);
                  }
                }
                else
                {
                  if (fmtSigned)
                  {
                    v = va_arg(args,int);
                  }
                  else
                  {
                    v = va_arg(args,unsigned int);
                  }
                }

                /* Strip sign */
                sign = 0;
                  /* (assumes sign bit is #31) */
                if( fmtSigned && (v & (0x80000000)) )
                  {
                  v = ~v + 1;
                  sign = 1;
                  }

                /* Count digits, and get largest place value */
                vShrink = v;
                p = 1;
                digitCount = 1;
                while( (vShrink = vShrink / fmtBase) > 0 )
                  {
                  digitCount++;
                  p *= fmtBase;
                  }

                /* Print leading characters & sign */
                fmtBeforeDecimal -= digitCount;
                if(fmtLeadingZero)
                  {
                  if(sign)
                    {
                    alt_log_txchar('-',(char*)base);
                    fmtBeforeDecimal--;
                    }
                  alt_log_repchar('0',fmtBeforeDecimal,base);
                  }
                else
                  {
                    if(sign)
                    {
                      fmtBeforeDecimal--;
                    }
                    alt_log_repchar(' ',fmtBeforeDecimal,base);
                    if(sign)
                    {
                      alt_log_txchar('-',(char*)base);
                    }
                  }

                /* Print numbery parts */
                while(p)
                  {
                  unsigned char d;

                  d = v / p;
                  d += '0';
                  if(d > '9')
                  {
                    d += (fmtCase ? 'A' : 'a') - '0' - 10;
                  }
                  alt_log_txchar(d,(char*)base);

                  v = v % p;
                  p = p / fmtBase;
                  }
                }

              state = pfState_chars;
              break;

            case 'u':
              fmtSigned = 0;
              goto doIntegerPrint;
            case 'o':
              fmtSigned = 0;
              fmtBase = 8;
              goto doIntegerPrint;
            case 'x':
              fmtSigned = 0;
              fmtBase = 16;
              goto doIntegerPrint;
            case 'X':
              fmtSigned = 0;
              fmtBase = 16;
              fmtCase = 1;
              goto doIntegerPrint;

            case 'c':
              alt_log_repchar(' ',fmtBeforeDecimal-1,base);
              alt_log_txchar(va_arg(args,int),(char*)base);
              break;

            case 's':
                {
                char *s;

                s = va_arg(args,char *);
                alt_log_repchar(' ',fmtBeforeDecimal-strlen(s),base);

                while(*s)
                  alt_log_txchar(*s++,(char*)base);
                }
              break;
            } /* switch last letter of fmt */
          state=pfState_chars;
          }
        break;
      } /* switch */
    } /* while chars left */
  } /* printf */

/* Main logging printf function */
int alt_log_printf_proc(const char *fmt, ... )
{
    va_list args;

    va_start (args, fmt);
    alt_log_private_printf(fmt,ALT_LOG_PORT_BASE,args);
    return (0);
}

/* Below are the functions called by different macros in various components. */

/* If the system has a JTAG_UART, include JTAG_UART debugging functions */
#ifdef __ALTERA_AVALON_JTAG_UART

/* The alarm function in altera_avalon_jtag_uart.c.
 * This function, when turned on, prints out the status
 * of the JTAG UART Control register, every ALT_LOG_JTAG_UART_TICKS.
 * If the flag is off, the alarm should never be registered, and this
 * function should never run */
alt_u32 altera_avalon_jtag_uart_report_log(void * context)
{
    if (alt_log_jtag_uart_alarm_on_flag) {
    altera_avalon_jtag_uart_state* dev = (altera_avalon_jtag_uart_state*) context;
        const char* header="JTAG Alarm:";
        alt_log_jtag_uart_print_control_reg(dev, dev->base, header);
        return ALT_LOG_JTAG_UART_TICKS;
    }
    else 
    {  
        /* If flag is not on, return 0 to disable future alarms.
        * Should never be here, alarm should not be enabled at all. */
        return 0;
    }
}

void alt_log_jtag_uart_print_control_reg(altera_avalon_jtag_uart_state* dev, int base, const char* header)
{
     unsigned int control, space, ac, wi, ri, we, re;
     control = IORD_ALTERA_AVALON_JTAG_UART_CONTROL(base);
     space = (control & ALTERA_AVALON_JTAG_UART_CONTROL_WSPACE_MSK) >>
             ALTERA_AVALON_JTAG_UART_CONTROL_WSPACE_OFST;
     we= (control & ALTERA_AVALON_JTAG_UART_CONTROL_WE_MSK) >>
         ALTERA_AVALON_JTAG_UART_CONTROL_WE_OFST;
     re= (control & ALTERA_AVALON_JTAG_UART_CONTROL_RE_MSK) >>
         ALTERA_AVALON_JTAG_UART_CONTROL_RE_OFST;
     ri= (control & ALTERA_AVALON_JTAG_UART_CONTROL_RI_MSK) >>
         ALTERA_AVALON_JTAG_UART_CONTROL_RI_OFST;
     wi= (control & ALTERA_AVALON_JTAG_UART_CONTROL_WI_MSK) >>
         ALTERA_AVALON_JTAG_UART_CONTROL_WI_OFST;
     ac= (control & ALTERA_AVALON_JTAG_UART_CONTROL_AC_MSK) >>
         ALTERA_AVALON_JTAG_UART_CONTROL_AC_OFST;
         
#ifdef ALTERA_AVALON_JTAG_UART_SMALL
    ALT_LOG_PRINTF(
     "%s HW FIFO wspace=%d AC=%d WI=%d RI=%d WE=%d RE=%d\r\n",
         header,space,ac,wi,ri,we,re);
#else
    ALT_LOG_PRINTF(
     "%s SW CirBuf = %d, HW FIFO wspace=%d AC=%d WI=%d RI=%d WE=%d RE=%d\r\n",
         header,(dev->tx_out-dev->tx_in),space,ac,wi,ri,we,re);
#endif   
         
     return;

}

/* In altera_avalon_jtag_uart.c
 * Same output as the alarm function above, but this is called in the driver
 * init function.  Hence, it gives the status of the JTAG UART control register
 * right at the initialization of the driver */ 
void alt_log_jtag_uart_startup_info(altera_avalon_jtag_uart_state* dev, int base)
{
     const char* header="JTAG Startup Info:";
     alt_log_jtag_uart_print_control_reg(dev, base, header);
     return;
}

/* In altera_avalon_jtag_uart.c
 * When turned on, this function will print out the status of the jtag uart
 * control register every time there is a jtag uart "almost-empty" interrupt. */
void alt_log_jtag_uart_isr_proc(int base, altera_avalon_jtag_uart_state* dev) 
{
    if (alt_log_jtag_uart_isr_on_flag) {
        const char* header="JTAG IRQ:";
        alt_log_jtag_uart_print_control_reg(dev, base, header);
    }
    return;
}

#endif /* __ALTERA_AVALON_JTAG_UART */ 

/* In alt_write.c
 * When the alt_log_write_on_flag is turned on, this function gets called
 * every time alt_write gets called.  The first 
 * ALT_LOG_WRITE_ECHO_LEN characters of every printf command (or any command
 * that eventually calls write()) gets echoed to the alt_log output. */
void alt_log_write(const void *ptr, size_t len)
{
    if (alt_log_write_on_flag) {
    int temp_cnt;
        int length=(ALT_LOG_WRITE_ECHO_LEN>len) ? len : ALT_LOG_WRITE_ECHO_LEN;

        if (length < 2) return;

        strncpy (alt_log_write_buf,ptr,length);
    alt_log_write_buf[length-1]='\n';
    alt_log_write_buf[length]='\r';
    alt_log_write_buf[length+1]='\0';

    /* Escape Ctrl-D's. If the Ctrl-D gets sent it might kill the terminal
         * connection of alt_log. It will get replaced by 'D'. */
        for (temp_cnt=0;temp_cnt < length; temp_cnt++) {
        if (alt_log_write_buf[temp_cnt]== 0x4) {
            alt_log_write_buf[temp_cnt]='D';
        }
    }
        ALT_LOG_PRINTF("Write Echo: %s",alt_log_write_buf);
    }
}

/* In altera_avalon_timer_sc
 * This function prints out a system clock is alive message
 * every ALT_LOG_SYS_CLK_INTERVAL (in ticks).  */
void alt_log_system_clock()
{
    if (alt_log_sys_clk_on_flag) {
    alt_log_sys_clk_count++;
        if (alt_log_sys_clk_count > ALT_LOG_SYS_CLK_INTERVAL) {
            alt_log_sys_clk_count = 0;
            ALT_LOG_PRINTF("System Clock On %u\r\n",alt_system_clock_in_sec++);
        }
    }
}


#endif
