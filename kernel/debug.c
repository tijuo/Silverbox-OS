#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <kernel/io.h>
#include <stdarg.h>
#include <kernel/lowlevel.h>
#include <os/syscalls.h>
#include <kernel/interrupt.h>
#include <kernel/bits.h>

#ifdef DEBUG

#define KITOA_BUF_LEN   12

#define SCREEN_HEIGHT   25
#define SCREEN_WIDTH    80

#define MISC_OUTPUT_RD_REG  0x3CC
#define MISC_OUTPUT_WR_REG  0x3C2
#define CRT_CONTR_INDEX     0x3D4
#define CRT_CONTR_DATA      0x3D5

#define START_ADDR_HIGH     0x0Cu
#define START_ADDR_LOW      0x0Du

#define CHARS_PER_LINE      SCREEN_WIDTH
#define MAX_LINES           203

#define DOWN_DIR            0
#define UP_DIR              1

#define CHAR_MASK           0x00FFu
#define COLOR_MASK          0xFF00u

enum TextColors { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GRAY,
  DARK_GRAY, LIGHT_BLUE, LIGHT_GREEN, LIGHT_CYAN, LIGHT_RED, LIGHT_MAGENTA, YELLOW,
  WHITE
};

#define COLOR_ATTR(fg, bg)  ((fg) | ((bg) << 4))
#define VGA_CHAR(c, fg, bg) (word)((c) | COLOR_ATTR(fg, bg) << 8)

#define VGA_CHAR_AT(vidmem, line, col)	(vidmem)[(CHARS_PER_LINE*(line)+(col))]
#define CHAR_OF(c)			((c) & 0xFF)
#define COLOR_OF(c)			(((c) >> 8) & 0xFF)

bool useLowMem;
bool badAssertHlt;
dword upper1;
dword lower1;
dword upper2;
dword lower2;

void kprintAt( const char *str, int x, int y );
void kprintf( const char *str, ... );
static int scroll(int up);
static void setScroll(word addr);
void resetScroll( void );
void scrollUp( void );
int scrollDown( void );
void doNewLine( int *x, int *y );
char *kitoa(int value, char *str, int base, int unSigned);
//char *klltoa(long long int value, char *str, int base, int unSigned);
void _putChar( char c, int x, int y, unsigned char attrib );
void putChar( char c, int x, int y );
void dump_regs( const tcb_t *thread, const ExecutionState *state, int intNum, int errorCode);
void dump_state( const ExecutionState *state, int intNum, int errorCode);
void dump_stack( addr_t, addr_t );
static const char *_digits="0123456789abcdefghijklmnopqrstuvwxyz";

unsigned int sx=0;
unsigned int sy=1;

#define COM1 0x3f8
#define COM2 0x2f8

#define COMBASE COM1

/** Sets up the serial ports for printing debug messages
    (and possibly receiving commands).
 */

void startTimeStamp(void)
{
  rdtsc(&upper1, &lower1);
}

void stopTimeStamp(void)
{
  rdtsc(&upper2, &lower2);
}

unsigned int getTimeDifference(void)
{
  if( upper1 == upper2 )
    return (unsigned)(lower2 - lower1);
  else if( upper2 == upper1 + 1 )
  {
    if( lower2 < lower1 )
      return (0xFFFFFFFFu - lower1) + lower2;
  }
  return 0xFFFFFFFFu;
}

void init_serial(void)
{
  outByte(COMBASE + 3, inByte(COMBASE + 3) | 0x80u); // Set DLAB for
  // DLLB access
  outByte(COMBASE, 0x03u);                           /* 38400 baud */
  outByte(COMBASE + 1, 0u); // Disable interrupts
  outByte(COMBASE + 3, inByte(COMBASE + 3) & 0x7fu); // Clear DLAB
  outByte(COMBASE + 3, 3u); // 8-N-1
}

int getDebugChar(void)
{
  while (!(inByte(COMBASE + 5) & 0x01u));
  return inByte(COMBASE);
}

void putDebugChar(int ch)
{
  outByte(0xE9, (byte)ch); // Use E9 hack to output characters
  while (!(inByte(COMBASE + 5) & 0x20u));
  outByte(COMBASE, (byte) ch);
  /*
if( ch == '\r' )
{
 sx=0;
 sy++;
 if( sy >= SCREEN_HEIGHT )
   scrollDown();
}
else if( ch == '\t' )
{
 sx +=8;
}
else
{
  putChar(ch,sx,sy);
  sx++;
}

if( sx >= SCREEN_WIDTH )
{
  sx=0;
  sy++;
  if( sy >= SCREEN_HEIGHT )
    scrollDown();
}
   */
}

char *kitoa(int value, char *str, int base, int unSigned)
{
  unsigned int num = (unsigned int)value;
  size_t str_end=0;
  int negative = base == 10 && !unSigned && (value & 0x80000000);

  if(negative)
    num = ~num + 1;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    do
    {
      unsigned int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );

    if(negative)
      str[str_end++] = '-';
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}

/*
char *klltoa(long long int value, char *str, int base, int unSigned)
{
  unsigned long long int num = (unsigned long long int)value;
  size_t str_end=0;
  int negative = !unSigned && (value & 0x8000000000000000ull);

  if(negative)
    num = ~num + 1;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    if(negative)
      str[str_end++] = '-';

    do
    {
      unsigned long long int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}
 */

/*
char *klltoa(long long int value, char *str, int base)
{
  unsigned long long int num = (unsigned long long int)value;
  size_t str_end=0;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    if( base == 10 && value < 0 )
    {
      str[str_end++] = '-';
    }
    do
    {
      unsigned long long int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}
 */
void initVideo( void )
{
  useLowMem = true;
  badAssertHlt = true;//false; // true;
  outByte(MISC_OUTPUT_WR_REG, inByte(MISC_OUTPUT_RD_REG) | FROM_FLAG_BIT(0));
}

/** Sets the flag to use conventional memory for printing instead
    of virtual memory.
 */
void setVideoLowMem( bool value )
{
  useLowMem = value;
}

/// Sets the flag to halt the system on a failed assertion

void setBadAssertHlt( bool value )
{
  badAssertHlt = value;
}

static void setScroll(word addr)
{
  byte high = (addr >> 8) & 0xFFu;
  byte low = addr & 0xFFu;

  outByte( CRT_CONTR_INDEX, START_ADDR_HIGH );
  outByte( CRT_CONTR_DATA, high );
  outByte( CRT_CONTR_INDEX, START_ADDR_LOW );
  outByte( CRT_CONTR_DATA, low );
}

void resetScroll( void )
{
  setScroll(0);
}

static int scroll(int up)
{
  int ret=0;
  byte high;
  byte low;
  word address;

  outByte( CRT_CONTR_INDEX, START_ADDR_HIGH );
  high = inByte( CRT_CONTR_DATA );
  outByte( CRT_CONTR_INDEX, START_ADDR_LOW );
  low = inByte( CRT_CONTR_DATA );
  address = (((word)high << 8) | low );

  if(up)
  {
    if(address == 0)
      address = CHARS_PER_LINE*MAX_LINES;
    else
      address -= CHARS_PER_LINE;
  }
  else
  {
    address += CHARS_PER_LINE;

    if( address > CHARS_PER_LINE*MAX_LINES )
    {
      address = 0;
      ret = 1;
    }
  }

  setScroll(address);

  return ret;
}

void scrollUp( void )
{
  scroll(UP_DIR);
}

int scrollDown( void )
{
  return scroll(DOWN_DIR);
}

void kprintAt( const char *str, int x, int y )
{
  volatile word *vidmem = (volatile word *)VIDMEM_START;

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  while(*str)
  {
    word w = VGA_CHAR_AT(vidmem, y, x);
    VGA_CHAR_AT(vidmem, y, x) = (w & COLOR_MASK) | *(str++);

    x = (x+1) % SCREEN_WIDTH;

    if(!x)
       y = (y+1) % MAX_LINES;
  }
}


void printAssertMsg(const char *exp, const char *file, const char *func, int line)
{
  kprintf("\n<'%s' %s: %d> assert(%s) failed\n", file, func, line, exp);

  if( currentThread )
    dump_regs((tcb_t *)currentThread, &currentThread->execState, -1, 0);

  if( badAssertHlt )
  {
    kprintf("\nSystem halted.");

    disableInt();
    asm("hlt");
  }
}

/** Increments a counter that keeps track of the number of times that
    the scheduler was called.
 */

CALL_COUNTER(incSchedCount)
void incSchedCount( void )
{
  volatile word *vidmem = ( volatile word * )(VIDMEM_START);
  char buf[KITOA_BUF_LEN];

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  word w = VGA_CHAR_AT(vidmem, 0, 0);

  VGA_CHAR_AT(vidmem, 0, 0) = VGA_CHAR(CHAR_OF(w + 1), RED, GRAY);

  assert( currentThread != NULL );

  if( currentThread )
  {
    kprintAt("tid:       pri:     quant:     cr3:         ", 2, 0);
    kprintAt( kitoa((int)getTid(currentThread), buf, 10, 0), 7, 0 );
    kprintAt( kitoa((int)currentThread->priority, buf, 10, 0), 18, 0 );
    kprintAt( kitoa((int)currentThread->quantaLeft, buf, 10, 0), 28, 0 );
    kprintAt( kitoa(currentThread->rootPageMap, buf, 16, 0), 37, 0 );
  }
}

/** Increments a counter that keeps track of the number of times that
    the timer interrupt was called.
 */

void incTimerCount( void )
{
  volatile word *vidmem = ( volatile word * ) ( VIDMEM_START );
  char buf[KITOA_BUF_LEN];

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  word w = VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1);

  VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH-1) = VGA_CHAR(CHAR_OF(w + 1), GREEN, GRAY);

  if(currentThread)
  {
    kprintAt( "   ", 28, 0);
    kprintAt( kitoa((int)currentThread->quantaLeft, buf, 10, 0), 28, 0 );
  }
}

/// Blanks the screen

void clearScreen( void )
{
  volatile word *vidmem = ( volatile word * ) ( VIDMEM_START );

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  for(int col=0; col < SCREEN_WIDTH; col++)
    VGA_CHAR_AT(vidmem, 0, col) = VGA_CHAR(' ', BLACK, GRAY);

  for(int line=1; line < SCREEN_HEIGHT; line++)
  {
    for(int col=0; col < SCREEN_WIDTH; col++)
      VGA_CHAR_AT(vidmem, line, col) = VGA_CHAR(' ', GRAY, BLACK);
  }

 // memset( (char *)vidmem, 0, SCREEN_HEIGHT * SCREEN_WIDTH * 2 );

  resetScroll();
}


void doNewLine( int *x, int *y )
{
  (*y)++;
  *x = 0;

  if(*y >= SCREEN_HEIGHT && scrollDown())
    *y = 0;
}


void _putChar( char c, int x, int y, unsigned char attrib )
{
  volatile word *vidmem = (volatile word *)(VIDMEM_START);

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  VGA_CHAR_AT(vidmem, y, x) = VGA_CHAR(c, attrib & 0x0F, (attrib >> 4) & 0x0F);
}

void putChar( char c, int x, int y )
{
  _putChar( c, x, y, COLOR_ATTR(GRAY, BLACK) );
}


/** A simplified stdio printf() for debugging use
    @param str The formatted string to print with format specifiers
    @param ... The arguments to the specifiers
 */

void kprintf( const char *str, ... )
{
  int longCounter=0;
  int unSigned=0;
  int percent = 0;
  va_list args;
  char buf[KITOA_BUF_LEN];
  size_t i;

  va_start(args, str);

  for(i=0; str[i]; i++ )
  {
    if( percent )
    {
      switch( str[i] )
      {
        case 'l':
          longCounter++;
          break;
        case '%':
          putDebugChar('%');
          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 'c':
          putDebugChar((char)va_arg(args,char));
          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 'p':
        case 'x':
        case 'X':
          if(longCounter >= 2)
          {
            long long int arg = va_arg(args, long long int);

            if(arg >> 32)
              kprintf(kitoa((int)(arg >> 32), buf, 16, 0));

            kprintf(kitoa((int)(arg & 0xFFFFFFFF), buf, 16, 0));
          }
          else
            kprintf(kitoa(va_arg(args, int), buf, 16, 0));

          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 's':
          kprintf(va_arg(args, char *));
          percent=0;
          longCounter=0;
          unSigned = 0;
          break;
        case 'u':
          unSigned = 1;
          break;
        case 'd':
        case 'i':
          /*
          if(longCounter >= 2)
            kprintf(klltoa(va_arg(args, long long int), buf, 10));
          else
           */
          kprintf(kitoa(va_arg(args, int), buf, 10, unSigned));

          percent = 0;
          longCounter = 0;
          unSigned = 0;
          break;
        default:
          break;
      }
    }
    else
    {
      switch( str[i] )
      {
        case '%':
          percent = 1;
          break;

        case '\n':
          //doNewLine( x, y );
          putDebugChar('\r');
          putDebugChar('\n');
          break;
          /*
        case '\r':
           *x = 0;
          break;
           */
        default:
          putDebugChar( str[i] );

          //  if( ++(*x) == SCREEN_WIDTH )
          //    doNewLine( x, y );
          break;
      }
    }
  }
  va_end(args);
}

void dump_state( const ExecutionState *execState, int intNum, int errorCode )
{
  if( execState == NULL )
  {
    kprintf("Unable to show execution state.\n");
    return;
  }

  if( intNum == SYSCALL_INT )
    kprintf("Syscall");
  else if( intNum < IRQ0 )
    kprintf("Exception %d", intNum);
  else if( intNum >= IRQ0 && intNum <= IRQ15 )
    kprintf("IRQ%d", intNum - IRQ0);
  else
    kprintf("Software Interrupt %d", intNum);

  kprintf(" @ EIP: 0x%x", execState->eip);

  kprintf( "\nEAX: 0x%x EBX: 0x%x ECX: 0x%x EDX: 0x%x", execState->eax, execState->ebx, execState->ecx, execState->edx );
  kprintf( "\nESI: 0x%x EDI: 0x%x EBP: 0x%x", execState->esi, execState->edi, execState->ebp );
  kprintf( "\nCS: 0x%x", execState->cs );

  if( intNum == PAGE_FAULT_INT )
    kprintf(" CR2: 0x%x", getCR2());

  kprintf( " error code: 0x%x\n", errorCode );

  kprintf("EFLAGS: 0x%x ", execState->eflags);

  if( execState->cs == UCODE )
    kprintf("ESP: 0x%x User SS: 0x%x\n", execState->userEsp, execState->userSS);
}

void dump_stack( addr_t stackFramePtr, addr_t addrSpace )
{
  kprintf("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*\n");

  while( stackFramePtr )
  {
    kprintf("<0x%x>:", stackFramePtr);

    if( isReadable( stackFramePtr + sizeof(dword), addrSpace ) )
      kprintf(" [0x%x]", *(dword *)(stackFramePtr + sizeof(dword)));
    else
      kprintf(" [???]");

    for( int i=2; i < 8; i++ )
    {
      if( isReadable( stackFramePtr + sizeof(dword) * i, addrSpace) )
        kprintf(" 0x%x", *(dword *)(stackFramePtr + sizeof(dword) * i));
      else
        break;
    }

    kprintf("\n");

    if( !isReadable(*(dword *)stackFramePtr, addrSpace) )
    {
      kprintf("<0x%x (invalid)>:\n", *(dword *)stackFramePtr);
      break;
    }
    else
      stackFramePtr = *(dword *)stackFramePtr;
  }
}

/** Prints useful debugging information about the current thread
  @param thread The current thread
  @param execState The saved execution state that was pushed to the stack upon context switch
  @param intNum The interrupt vector (if applicable)
  @param errorCode The error code provided by the processor (if applicable)
*/

void dump_regs( const tcb_t *thread, const ExecutionState *execState, int intNum, int errorCode )
{
  addr_t stackFramePtr;

  kprintf( "Thread: 0x%x (TID: %d) ", thread, getTid(thread));

  dump_state(execState, intNum, errorCode);

  if( !execState )
    kprintf("\n");

  kprintf( "Thread CR3: 0x%x Current CR3: 0x%x\n", *(unsigned *)&thread->rootPageMap, getCR3() );

  if( !execState || intNum < 0)
  {
    __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));

    if( !isReadable(*(dword *)stackFramePtr, thread->rootPageMap) )
    {
      kprintf("Unable to dump the stack\n");
      return;
    }

    stackFramePtr = *(addr_t *)stackFramePtr;
  }
  else
    stackFramePtr = (addr_t)execState->ebp;

  dump_stack(stackFramePtr, thread->rootPageMap);
}

#endif /* DEBUG */

void abort(void);

void abort(void)
{
  addr_t stackFramePtr;

  kprintf("Debug Error: abort() has been called.\n");
  __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));
  dump_stack(stackFramePtr, getRootPageMap());

  __asm__("hlt");
}

void printPanicMsg(const char *msg, const char *file, const char *func, int line)
{
  addr_t stackFramePtr;
  kprintf("\nKernel panic - %s(): %d (%s). %s\nSystem halted\n", func, line, file, msg);

  __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));
  dump_stack(stackFramePtr, getRootPageMap());

  disableInt();
  __asm__("hlt");
  disableInt();
}
