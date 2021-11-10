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
#include <string.h>
#include <limits.h>
#include <stdint.h>

#ifdef DEBUG

#define KITOA_BUF_LEN   65

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

enum TextColors {
  BLACK,
  BLUE,
  GREEN,
  CYAN,
  RED,
  MAGENTA,
  BROWN,
  GRAY,
  DARK_GRAY,
  LIGHT_BLUE,
  LIGHT_GREEN,
  LIGHT_CYAN,
  LIGHT_RED,
  LIGHT_MAGENTA,
  YELLOW,
  WHITE
};

#define COLOR_ATTR(fg, bg)  ((fg) | ((bg) << 4))
#define VGA_CHAR(c, fg, bg) (word)((c) | COLOR_ATTR(fg, bg) << 8)

#define VGA_CHAR_AT(vidmem, line, col)	(vidmem)[(CHARS_PER_LINE*(line)+(col))]
#define CHAR_OF(c)			((c) & 0xFF)
#define COLOR_OF(c)			(((c) >> 8) & 0xFF)

#define itoa(value, str, base)		_Generic((value), \
    long long int: kllitoa, \
    unsigned long long int: kullitoa, \
    unsigned int: kuitoa, \
    long int: klitoa, \
    unsigned long int: kulitoa, \
default: kitoa)((value), (str), (base))

enum PrintfLength {
  DEFAULT = 0,
  CHAR,
  SHORT_INT,
  LONG_INT,
  LONG_LONG_INT,
  INTMAX,
  SIZE,
  PTRDIFF,
  LONG_DOUBLE
};
enum PrintfParseState {
  FLAGS = 0, WIDTH, PRECISION, LENGTH, SPECIFIER
};

struct PrintfState {
  unsigned short int isZeroPad :1;
  unsigned short int usePrefix :1;
  unsigned short int isLeftJustify :1;
  unsigned short int isForcedPlus :1;
  unsigned short int isBlankPlus :1;
  unsigned short int inPercent :1;
  unsigned short int isError :1;
  unsigned short int isNegative :1;
  unsigned short int printfState :4;
  unsigned short int length :4;
  char prefix[2];
  size_t width;
  int precision;
  size_t percentIndex;
  size_t argLength;
  char *string;
};

bool badAssertHlt;
dword upper1;
dword lower1;
dword upper2;
dword lower2;

unsigned int cursorX = 0;
unsigned int cursorY = 0;

void resetPrintfState(struct PrintfState *state);

static void printChar(int c);
static void _kprintf(void (*writeFunc)(int), const char *str, va_list args);
void kprintf(const char *str, ...) __attribute__((format(printf, 1, 2)));
void kprintfAt(unsigned int x, unsigned int y, const char *str, ...) __attribute__((format(printf, 3, 4)));
static int scroll(int up);
static void setScroll(word addr);
void resetScroll(void);
void scrollUp(void);
int scrollDown(void);
void doNewLine(int *x, int *y);
int kitoa(int value, char *str, int base);
int kuitoa(unsigned int value, char *str, int base);
int klitoa(long int value, char *str, int base);
int kulitoa(unsigned long int value, char *str, int base);
int kllitoa(long long int value, char *str, int base);
int kullitoa(unsigned long long int value, char *str, int base);

void _putChar(char c, int x, int y, unsigned char attrib);
void putChar(char c, int x, int y);
void dump_regs(const tcb_t *thread, const ExecutionState *state, unsigned int intNum,
               unsigned int errorCode);
void dump_state(const ExecutionState *state, unsigned int intNum, unsigned int errorCode);
void dump_stack(addr_t, addr_t);
static const char *_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

unsigned int sx = 0;
unsigned int sy = 1;

#define COM1 0x3f8
#define COM2 0x2f8

#define COMBASE COM1

/** Sets up the serial ports for printing debug messages
 (and possibly receiving commands).
 */

void startTimeStamp(void) {
  rdtsc(&upper1, &lower1);
}

void stopTimeStamp(void) {
  rdtsc(&upper2, &lower2);
}

unsigned int getTimeDifference(void) {
  if(upper1 == upper2)
    return (unsigned)(lower2 - lower1);
  else if(upper2 == upper1 + 1) {
    if(lower2 < lower1)
      return (0xFFFFFFFFu - lower1) + lower2;
  }
  return 0xFFFFFFFFu;
}

void init_serial(void) {
  outByte(COMBASE + 3, inByte(COMBASE + 3) | 0x80u); // Set DLAB for
  // DLLB access
  outByte(COMBASE, 0x03u); /* 38400 baud */
  outByte(COMBASE + 1, 0u); // Disable interrupts
  outByte(COMBASE + 3, inByte(COMBASE + 3) & 0x7fu); // Clear DLAB
  outByte(COMBASE + 3, 3u); // 8-N-1
}

int getDebugChar(void) {
  while(!(inByte(COMBASE + 5) & 0x01u))
    ;
  return inByte(COMBASE);
}

void putDebugChar(int ch) {
  outByte(0xE9, (byte)ch); // Use E9 hack to output characters
  while(!(inByte(COMBASE + 5) & 0x20u))
    ;
  outByte(COMBASE, (byte)ch);
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

int kitoa(int value, char *str, int base) {
  size_t strEnd = 0;
  int negative = base == 10 && value < 0;
  int charsWritten = 0;

  if(!str || base < 2 || base > 36)
    return -1;

  if(negative) {
    if(value == INT_MIN) {
      memcpy(str, "-2147483648", 11);
      return 11;
    }
    else
      value = ~value + 1;
  }

  do {
    unsigned int quot = base * ((unsigned int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while(value);

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int kuitoa(unsigned int value, char *str, int base) {
  size_t strEnd = 0;
  int charsWritten = 0;

  if(!str || base < 2 || base > 36)
    return -1;

  do {
    unsigned int quot = base * ((unsigned int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while(value);

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int klitoa(long int value, char *str, int base) {
  size_t strEnd = 0;
  int negative = base == 10 && value < 0;
  int charsWritten = 0;

  if(!str || base < 2 || base > 36)
    return -1;

  if(negative) {
    if(value == (long int)LONG_MIN) {
#ifdef __LONG_LEN__
#if __LONG_LEN__ == 8
      memcpy(str, "-9223372036854775808", 20);
      return 20;
#else
      memcpy(str, "-2147483648", 11);
      return 11;
#endif /* __LONG__LEN == 8 */
#else
      memcpy(str, "-2147483648", 11);
      return 11;
#endif /* __LONG_LEN__ */
    }
    else
      value = ~value + 1;
  }

  do {
    long int quot = base * (value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while(value);

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int kulitoa(unsigned long int value, char *str, int base) {
  size_t strEnd = 0;
  int charsWritten = 0;

  if(!str || base < 2 || base > 36)
    return -1;

  do {
    unsigned long int quot = base * ((unsigned long int)value / base);

    str[strEnd++] = _digits[value - quot];
    value = quot / base;
  } while(value);

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int kllitoa(long long int value, char *str, int base) {
  size_t strEnd = 0;
  int negative = base == 10 && value < 0;
  int charsWritten = 0;
  unsigned int v[2] = {
      (unsigned int)(value & 0xFFFFFFFFu), (unsigned int)(value >> 32) };

  assert(base == 2 || base == 4 || base == 8 || base == 16 || base == 32);

  if(!str || base < 2 || base > 36)
    return -1;

  if(negative) {
    if(value == (long long int)LLONG_MIN) {
      memcpy(str, "-9223372036854775808", 20);
      return 20;
    }
    else
      value = ~value + 1;
  }

  for(int i = 0; i < 2; i++) {
    if(i == 1 && v[i] == 0)
      break;

    do {
      unsigned int quot = base * ((unsigned int)v[i] / base);

      str[strEnd++] = _digits[v[i] - quot];
      v[i] = quot / base;
    } while(v[i]);
  }

  if(negative)
    str[strEnd++] = '-';

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

int kullitoa(unsigned long long int value, char *str, int base) {
  size_t strEnd = 0;
  int charsWritten = 0;
  unsigned int v[2] = {
      (unsigned int)(value & 0xFFFFFFFFu), (unsigned int)(value >> 32) };

  if(!str || base < 2 || base > 36)
    return -1;

  for(int i = 0; i < 2; i++) {
    if(i == 1 && v[i] == 0)
      break;

    do {
      unsigned int quot = base * ((unsigned int)v[i] / base);

      str[strEnd++] = _digits[v[i] - quot];
      v[i] = quot / base;
    } while(v[i]);
  }

  charsWritten = strEnd;

  if(strEnd) {
    strEnd--;

    for(size_t i = 0; i < strEnd; i++, strEnd--) {
      char tmp = str[i];
      str[i] = str[strEnd];
      str[strEnd] = tmp;
    }
  }

  return charsWritten;
}

void initVideo(void) {
  badAssertHlt = true; //false; // true;
  outByte(MISC_OUTPUT_WR_REG, inByte(MISC_OUTPUT_RD_REG) | FROM_FLAG_BIT(0));
}

/// Sets the flag to halt the system on a failed assertion

void setBadAssertHlt( bool value) {
  badAssertHlt = value;
}

static void setScroll(word addr) {
  byte high = (addr >> 8) & 0xFFu;
  byte low = addr & 0xFFu;

  outByte( CRT_CONTR_INDEX, START_ADDR_HIGH);
  outByte( CRT_CONTR_DATA, high);
  outByte( CRT_CONTR_INDEX, START_ADDR_LOW);
  outByte( CRT_CONTR_DATA, low);
}

void resetScroll(void) {
  setScroll(0);
}

static int scroll(int up) {
  int ret = 0;
  byte high;
  byte low;
  word address;

  outByte( CRT_CONTR_INDEX, START_ADDR_HIGH);
  high = inByte( CRT_CONTR_DATA);
  outByte( CRT_CONTR_INDEX, START_ADDR_LOW);
  low = inByte( CRT_CONTR_DATA);
  address = (((word)high << 8) | low);

  if(up) {
    if(address == 0)
      address = CHARS_PER_LINE * MAX_LINES;
    else
      address -= CHARS_PER_LINE;
  }
  else {
    address += CHARS_PER_LINE;

    if(address > CHARS_PER_LINE * MAX_LINES) {
      address = 0;
      ret = 1;
    }
  }

  setScroll(address);

  return ret;
}

void scrollUp(void) {
  scroll(UP_DIR);
}

int scrollDown(void) {
  return scroll(DOWN_DIR);
}

void kprintfAt(unsigned int x, unsigned int y, const char *str, ...) {
  unsigned int _cursorX = cursorX, _cursorY = cursorY;
  va_list args;

  cursorX = x;
  cursorY = y;

  va_start(args, str);
  _kprintf(&printChar, str, args);
  va_end(args);

  cursorX = _cursorX;
  cursorY = _cursorY;
}

void printAssertMsg(const char *exp, const char *file, const char *func,
                    int line) {
  tcb_t *currentThread = getCurrentThread();

  kprintf("\n<'%s' %s: %d> assert(%s) failed\n", file, func, line, exp);

  if(currentThread)
    dump_regs((tcb_t*)currentThread, &currentThread->userExecState, -1, 0);

  if(badAssertHlt) {
    kprintf("\nSystem halted.");

    disableInt();
    asm("hlt");
  }
}

/** Increments a counter that keeps track of the number of times that
 the scheduler was called.
 */

CALL_COUNTER(incSchedCount)
void incSchedCount(void) {
  tcb_t *currentThread = getCurrentThread();

  volatile word *vidmem = (volatile word*)(VIDMEM_START);

  word w = VGA_CHAR_AT(vidmem, 0, 0);

  VGA_CHAR_AT(vidmem, 0, 0)= VGA_CHAR(CHAR_OF(w + 1), RED, GRAY);

  assert( currentThread != NULL);

  if(currentThread) {
    kprintfAt(2, 0, "tid: %5u cr3: %p", getTid(currentThread),
              (void*)currentThread->rootPageMap);
  }
}

/** Increments a counter that keeps track of the number of times that
 the timer interrupt was called.
 */

void incTimerCount(void) {
  volatile word *vidmem = (volatile word*)( VIDMEM_START);

  word w = VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1);

  VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH-1)= VGA_CHAR(CHAR_OF(w + 1), GREEN, GRAY);
}

/// Blanks the screen

void clearScreen(void) {
  volatile word *vidmem = (volatile word*)( VIDMEM_START);

  for(int col = 0; col < SCREEN_WIDTH; col++)
    VGA_CHAR_AT(vidmem, 0, col)= VGA_CHAR(' ', BLACK, GRAY);

  for(int line = 1; line < SCREEN_HEIGHT; line++) {
    for(int col = 0; col < SCREEN_WIDTH; col++)
      VGA_CHAR_AT(vidmem, line, col)= VGA_CHAR(' ', GRAY, BLACK);
    }

    // memset( (char *)vidmem, 0, SCREEN_HEIGHT * SCREEN_WIDTH * 2 );

  resetScroll();
}

void doNewLine(int *x, int *y) {
  (*y)++;
  *x = 0;

  if(*y >= SCREEN_HEIGHT && scrollDown())
    *y = 0;
}

void printChar(int c) {
  volatile word *vidmem = (volatile word*)VIDMEM_START + cursorY * SCREEN_WIDTH
                          + cursorX;

  *vidmem = (*vidmem & 0xFF00) | (unsigned char)c;

  cursorX++;

  if(cursorX >= SCREEN_WIDTH) {
    cursorX = 0;
    cursorY++;
  }
}

void _putChar(char c, int x, int y, unsigned char attrib) {
  volatile word *vidmem = (volatile word*)(VIDMEM_START);

  VGA_CHAR_AT(vidmem, y, x)= VGA_CHAR(c, attrib & 0x0F, (attrib >> 4) & 0x0F);
}

void putChar(char c, int x, int y) {
  _putChar(c, x, y, COLOR_ATTR(GRAY, BLACK));
}

void resetPrintfState(struct PrintfState *state) {
  clearMemory(state, sizeof *state);
  state->precision = -1;
}

void kprintf(const char *str, ...) {
  va_list args;

  va_start(args, str);
  _kprintf(&putDebugChar, str, args);
  va_end(args);
}

/** A simplified stdio printf() for debugging use
 @param writeFunc The function to be used to output characters.
 @param str The formatted string to print with format specifiers
 @param ... The arguments to the specifiers
 */

void _kprintf(void (*writeFunc)(int), const char *str, va_list args) {
  struct PrintfState state;
  char buf[KITOA_BUF_LEN];
  size_t i;
  int charsWritten = 0;

  resetPrintfState(&state);

  for(i = 0; str[i]; i++) {
    if(state.inPercent) {
      switch(str[i]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          switch(state.printfState) {
            case FLAGS:
            case WIDTH:
              if(state.printfState == FLAGS) {
                if(str[i] == '0')
                  state.isZeroPad = 1;

                state.printfState = WIDTH;
              }

              state.width = 10 * state.width + str[i] - '0';
              continue;
            case PRECISION:
              state.precision = 10 * state.precision + str[i] - '0';
              continue;
            case LENGTH:
            default:
              state.isError = 1;
              break;
          }
          break;
        case 'l':
          if(state.printfState == LENGTH) {
            state.length = LONG_LONG_INT;
            state.printfState = SPECIFIER;
            continue;
          }
          else if(state.printfState < LENGTH) {
            state.length = LONG_INT;
            state.printfState = LENGTH;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 'h':
          if(state.printfState == LENGTH) {
            state.length = SHORT_INT;
            state.printfState = SPECIFIER;
            continue;
          }
          else if(state.printfState < LENGTH) {
            state.length = CHAR;
            state.printfState = LENGTH;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 'j':
          if(state.printfState != SPECIFIER) {
            state.length = INTMAX;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 'z':
          if(state.printfState != SPECIFIER) {
            state.length = SIZE;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 't':
          if(state.printfState != SPECIFIER) {
            state.length = PTRDIFF;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 'L':
          if(state.printfState != SPECIFIER) {
            state.length = LONG_DOUBLE;
            state.printfState = SPECIFIER;
            continue;
          }
          else {
            state.isError = 1;
            break;
          }
          break;
        case 'c':
        case '%':
          if(str[i] == 'c') {/*
           if(state.length == LONG_INT)
           buf[0] = (char)va_arg(args, wint_t);
           else
           */
            buf[0] = (char)va_arg(args, int);
          }
          else
            buf[0] = '%';

          state.string = buf;
          state.argLength = 1;
          break;
        case '.':
          switch(state.printfState) {
            case FLAGS:
            case WIDTH:
              state.precision = 0;
              state.printfState = PRECISION;
              continue;
            default:
              state.isError = 1;
              break;
          }
          break;
        case 'p':
        case 'x':
        case 'X':
          if(str[i] == 'p')
            state.precision = 2 * sizeof(void*);

          switch(state.length) {
            case LONG_LONG_INT:
              state.argLength = itoa(va_arg(args, unsigned long long int), buf, 16);
              break;
            case CHAR:
              state.argLength = itoa((unsigned char)va_arg(args, unsigned int), buf, 16);
              break;
            case SHORT_INT:
              state.argLength = itoa((unsigned short int)va_arg(args, unsigned int), buf, 16);
              break;
            case LONG_INT:
              state.argLength = itoa((unsigned long int)va_arg(args, unsigned long int), buf, 16);
              break;
            case DEFAULT:
            default:
              state.argLength = itoa(va_arg(args, unsigned int), buf, 16);
              break;
          }

          if(state.usePrefix) {
            state.prefix[0] = '0';
            state.prefix[1] = str[i] != 'X' ? 'x' : 'X';
          }

          if(str[i] == 'X') {
            for(size_t pos = 0; pos < state.argLength; pos++) {
              if(buf[pos] >= 'a' && buf[pos] <= 'z')
                buf[pos] -= 'a' - 'A';
            }
          }

          state.string = buf;
          break;
        case 'o':
          switch(state.length) {
            case LONG_LONG_INT:
              state.argLength = itoa(va_arg(args, unsigned long long int), buf, 8);
              break;
            case CHAR:
              state.argLength = itoa((unsigned char)va_arg(args, unsigned int), buf, 8);
              break;
            case SHORT_INT:
              state.argLength = itoa((unsigned short int)va_arg(args, unsigned int), buf, 8);
              break;
            case LONG_INT:
              state.argLength = itoa((unsigned long int)va_arg(args, unsigned long int), buf, 8);
              break;
            case DEFAULT:
            default:
              state.argLength = itoa(va_arg(args, unsigned int), buf, 8);
              break;
          }

          if(state.usePrefix) {
            state.prefix[0] = '0';
            state.prefix[1] = '\0';
          }

          state.string = buf;
          break;
        case 's':
          state.string = va_arg(args, char*);

          if(state.precision == -1)
            for(state.argLength = 0; state.string[state.argLength];
                state.argLength++)
              ;
          else
            state.argLength = state.precision;

          break;
        case 'u':
          switch(state.length) {
            case LONG_LONG_INT:
              state.argLength = itoa(va_arg(args, unsigned long long int), buf, 10);
              break;
            case CHAR:
              state.argLength = itoa((unsigned char)va_arg(args, unsigned int), buf, 10);
              break;
            case SHORT_INT:
              state.argLength = itoa((unsigned short int)va_arg(args, unsigned int), buf, 10);
              break;
            case LONG_INT:
              state.argLength = itoa((unsigned long int)va_arg(args, unsigned long int), buf, 10);
              break;
            case DEFAULT:
            default:
              state.argLength = itoa(va_arg(args, unsigned int), buf, 10);
              break;
          }

          state.string = buf;
          break;
        case 'd':
        case 'i':
          switch(state.length) {
            case LONG_LONG_INT:
              state.argLength = itoa(va_arg(args, long long int), buf, 10);
              break;
            case CHAR:
              state.argLength = itoa((signed char)va_arg(args, int), buf, 10);
              break;
            case SHORT_INT:
              state.argLength = itoa((short int)va_arg(args, int), buf, 10);
              break;
            case LONG_INT:
              state.argLength = itoa((long int)va_arg(args, long int), buf, 10);
              break;
            case DEFAULT:
            default:
              state.argLength = itoa(va_arg(args, int), buf, 10);
              break;
          }

          state.isNegative = buf[0] == '-';
          state.string = buf;

          break;
        case '#':
          if(state.printfState == FLAGS) {
            state.usePrefix = 1;
            continue;
          }
          else
            state.isError = 1;
          break;
        case '-':
          if(state.printfState == FLAGS) {
            state.isLeftJustify = 1;
            continue;
          }
          else
            state.isError = 1;
          break;
        case '+':
          if(state.printfState == FLAGS) {
            state.isForcedPlus = 1;
            continue;
          }
          else
            state.isError = 1;
          break;
        case ' ':
          if(state.printfState == FLAGS) {
            state.isBlankPlus = 1;
            continue;
          }
          else
            state.isError = 1;
          break;
        case '*':
          switch(state.printfState) {
            case WIDTH:
              state.width = (size_t)va_arg(args, int);
              continue;
            case PRECISION:
              state.precision = (size_t)va_arg(args, int);
              continue;
            default:
              state.isError = 1;
              break;
          }
          break;
        case 'n':
          if(state.printfState == WIDTH || state.printfState == PRECISION
             || state.isForcedPlus || state.isBlankPlus || state.isZeroPad
             || state.usePrefix || state.isLeftJustify) {
            state.isError = 1;
            break;
          }
          else {
            switch(state.length) {
              case LONG_LONG_INT:
                *va_arg(args, long long int*) = charsWritten;
                break;
              case CHAR:
                *va_arg(args, signed char*) = charsWritten;
                break;
              case SHORT_INT:
                *va_arg(args, short int*) = charsWritten;
                break;
              case LONG_INT:
                *va_arg(args, long int*) = charsWritten;
                break;
              case INTMAX:
                *va_arg(args, intmax_t*) = charsWritten;
                break;
              case SIZE:
                *va_arg(args, size_t*) = charsWritten;
                break;
              case PTRDIFF:
                *va_arg(args, ptrdiff_t*) = charsWritten;
                break;
              case DEFAULT:
              default:
                *va_arg(args, int*) = charsWritten;
                break;
            }
          }
          continue;
        default:
          state.isError = 1;
          break;
      }

      if(state.isError) {
        for(size_t j = state.percentIndex; j <= i; j++, charsWritten++)
          writeFunc(str[j]);
      }
      else {
        size_t argLength = state.argLength - (state.isNegative ? 1 : 0);
        size_t signLength =
            (state.isNegative || state.isForcedPlus || state.isBlankPlus) ? 1 :
                                                                            0;
        size_t prefixLength;
        size_t zeroPadLength;
        size_t spacePadLength;

        if(state.usePrefix)
          prefixLength =
              state.prefix[0] == '\0' ? 0 : (state.prefix[1] == '\0' ? 1 : 2);
        else
          prefixLength = 0;

        if(str[i] == 's' || state.precision == -1 || argLength > INT_MAX
           || (int)argLength >= state.precision)
          zeroPadLength = 0;
        else
          zeroPadLength = state.precision - argLength;

        if(argLength + signLength + prefixLength + zeroPadLength < state.width)
          spacePadLength = state.width - argLength - signLength - prefixLength
                           - zeroPadLength;
        else
          spacePadLength = 0;

        if(!state.isLeftJustify) {
          for(; spacePadLength > 0; spacePadLength--, charsWritten++)
            writeFunc(' ');
        }

        if(state.isNegative) {
          writeFunc('-');
          charsWritten++;
        }
        else if(state.isForcedPlus) {
          writeFunc('+');
          charsWritten++;
        }
        else if(state.isBlankPlus) {
          writeFunc(' ');
          charsWritten++;
        }

        for(size_t pos = 0; pos < prefixLength; pos++, charsWritten++)
          writeFunc(state.prefix[pos]);

        for(; zeroPadLength > 0; zeroPadLength--, charsWritten++)
          writeFunc('0');

        for(size_t pos = (state.isNegative ? 1 : 0); argLength > 0;
            argLength--, pos++, charsWritten++)
          writeFunc(state.string[pos]);

        if(state.isLeftJustify) {
          for(; spacePadLength > 0; spacePadLength--, charsWritten++)
            writeFunc(' ');
        }
      }
      resetPrintfState(&state);
    }
    else {
      switch(str[i]) {
        case '%':
          state.percentIndex = i;
          state.inPercent = 1;
          memset(buf, 0, KITOA_BUF_LEN);
          break;
        case '\n':
          writeFunc('\r');
          writeFunc('\n');
          charsWritten += 2;
          break;
        case '\r':
          writeFunc('\r');
          charsWritten++;
          break;
        default:
          writeFunc(str[i]);
          charsWritten++;
          break;
      }
    }
  }
}

void dump_state(const ExecutionState *execState, unsigned int intNum, unsigned int errorCode) {
  if(execState == NULL) {
    kprintf("Unable to show execution state.\n");
    return;
  }

  if(intNum < IRQ_BASE)
    kprintf("Exception %u", intNum);
  else
    kprintf("IRQ%u", intNum - IRQ_BASE);

  kprintf(" @ EIP: 0x%lx", execState->eip);

  kprintf("\nEAX: 0x%lx EBX: 0x%lx ECX: 0x%lx EDX: 0x%lx", execState->eax,
          execState->ebx, execState->ecx, execState->edx);
  kprintf("\nESI: 0x%lx EDI: 0x%lx EBP: 0x%lx", execState->esi, execState->edi,
          execState->ebp);
  kprintf("\nCS: 0x%hhx", execState->cs);

  if(intNum == PAGE_FAULT_INT)
    kprintf(" CR2: 0x%lx", getCR2());

  kprintf(" error code: 0x%x\n", errorCode);

  kprintf("EFLAGS: 0x%lx ", execState->eflags);

  if(execState->cs == UCODE_SEL)
    kprintf("ESP: 0x%lx User SS: 0x%hhx\n", execState->userEsp, execState->userSS);
}

void dump_stack(addr_t stackFramePtr, addr_t addrSpace) {
  kprintf("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*\n");

  while(stackFramePtr) {
    kprintf("<0x%lx>:", stackFramePtr);

    if(isReadable(stackFramePtr + sizeof(dword), addrSpace))
      kprintf(" [0x%lx]", *(dword*)(stackFramePtr + sizeof(dword)));
    else
      kprintf(" [???]");

    for(int i = 2; i < 8; i++) {
      if(isReadable(stackFramePtr + sizeof(dword) * i, addrSpace))
        kprintf(" 0x%lx", *(dword*)(stackFramePtr + sizeof(dword) * i));
      else
        break;
    }

    kprintf("\n");

    if(!isReadable(*(dword*)stackFramePtr, addrSpace)) {
      kprintf("<0x%lx (invalid)>:\n", *(dword*)stackFramePtr);
      break;
    }
    else
      stackFramePtr = *(dword*)stackFramePtr;
  }
}

/** Prints useful debugging information about the current thread
 @param thread The current thread
 @param execState The saved execution state that was pushed to the stack upon context switch
 @param intNum The interrupt vector (if applicable)
 @param errorCode The error code provided by the processor (if applicable)
 */

void dump_regs(const tcb_t *thread, const ExecutionState *execState, unsigned int intNum,
               unsigned int errorCode) {
  dword stackFramePtr;

  kprintf("Thread: %p (TID: %u) ", thread, getTid(thread));

  dump_state(execState, intNum, errorCode);

  if(!execState)
    kprintf("\n");

  kprintf("Thread CR3: %#lx Current CR3: %#lx\n",
          *(const uint32_t *)&thread->rootPageMap, getCR3());

  if(!execState) {
    __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));

    if(!isReadable(*(dword*)stackFramePtr, thread->rootPageMap)) {
      kprintf("Unable to dump the stack\n");
      return;
    }

    stackFramePtr = *(dword*)stackFramePtr;
  }
  else
    stackFramePtr = (dword)execState->ebp;

  dump_stack((addr_t)stackFramePtr, (addr_t)thread->rootPageMap);
}

#endif /* DEBUG */

noreturn void abort(void);

noreturn void abort(void) {
  addr_t stackFramePtr;

  kprintf("Debug Error: abort() has been called.\n");
  __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));
  dump_stack(stackFramePtr, getRootPageMap());

  while(1)
  {
    disableInt();
    halt();
  }
}

noreturn void printPanicMsg(const char *msg, const char *file, const char *func,
                            int line) {
  addr_t stackFramePtr;
  kprintf("\nKernel panic - %s(): %d (%s). %s\nSystem halted\n", func, line,
          file, msg);

  __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));
  dump_stack(stackFramePtr, getRootPageMap());

  while(1)
  {
    disableInt();
    halt();
  }
}
