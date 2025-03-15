#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <os/io.h>
#include <stdarg.h>
#include <kernel/lowlevel.h>
#include <os/syscalls.h>
#include <kernel/interrupt.h>
#include <kernel/bits.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#ifdef DEBUG

#define VIDMEM_START KPHYS_TO_VIRT(0xB8000u)

#define KITOA_BUF_LEN 65

#define SCREEN_HEIGHT 25
#define SCREEN_WIDTH 80

#define MISC_OUTPUT_RD_REG 0x3CC
#define MISC_OUTPUT_WR_REG 0x3C2
#define CRT_CONTR_INDEX 0x3D4
#define CRT_CONTR_DATA 0x3D5

#define START_ADDR_HIGH 0x0Cu
#define START_ADDR_LOW 0x0Du

#define CHARS_PER_LINE SCREEN_WIDTH
#define MAX_LINES 203

#define DOWN_DIR 0
#define UP_DIR 1

#define CHAR_MASK 0x00FFu
#define COLOR_MASK 0xFF00u

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

#define COLOR_ATTR(fg, bg) ((fg) | ((bg) << 4))
#define VGA_CHAR(c, fg, bg) (word)((c) | COLOR_ATTR(fg, bg) << 8)

#define VGA_CHAR_AT(vidmem, line, col) (vidmem)[(CHARS_PER_LINE * (line) + (col))]
#define CHAR_OF(c) ((c) & 0xFFu)
#define COLOR_OF(c) (((c) >> 8) & 0xFFu)

// int itoa(value, char *str, int base);
#define itoa(value, str, base) _Generic((value), \
    long long int: kllitoa,                      \
    unsigned long long int: kullitoa,            \
    unsigned int: kuitoa,                        \
    long int: klitoa,                            \
    unsigned long int: kulitoa,                  \
    default: kitoa)((value), (str), (base))

#define DEFAULT_PRECISION -1

enum PrintfLength {
    DEFAULT,
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
    FLAGS,
    WIDTH,
    PRECISION,
    LENGTH,
    SPECIFIER
};

struct PrintfState {
    unsigned short int is_zero_pad : 1;
    unsigned short int use_prefix : 1;
    unsigned short int is_left_justify : 1;
    unsigned short int is_forced_plus : 1;
    unsigned short int is_blank_plus : 1;
    unsigned short int inPercent : 1;
    unsigned short int is_error : 1;
    unsigned short int is_negative : 1;
    unsigned short int printf_state : 4;
    unsigned short int length : 4;
    char prefix[2];
    size_t width;
    int precision;
    size_t percent_index;
    size_t arg_length;
    char* string;
};

bool bad_assert_hlt;
dword upper1;
dword lower1;
dword upper2;
dword lower2;

unsigned int cursor_x = 0;
unsigned int cursor_y = 0;

NON_NULL_PARAMS void reset_printf_state(struct PrintfState* state);

static void print_char(int c);
NON_NULL_PARAMS static void _kprintf(void (*write_func)(int), const char* str,
    va_list args);
NON_NULL_PARAM(1)
void kprintf(const char* str, ...) __attribute__((format(printf, 1, 2)));
NON_NULL_PARAM(3)
void kprintf_at(unsigned int x, unsigned int y,
    const char* str, ...) __attribute__((format(printf, 3, 4)));
static int scroll(int up);
static void set_scroll(word addr);
void reset_scroll(void);
void scroll_up(void);
int scroll_down(void);
NON_NULL_PARAMS void do_new_line(int* x, int* y);
int kitoa(int value, char* str, int base);
NON_NULL_PARAMS int kuitoa(unsigned int value, char* str, int base);
NON_NULL_PARAMS int klitoa(long int value, char* str, int base);
NON_NULL_PARAMS int kulitoa(unsigned long int value, char* str, int base);
NON_NULL_PARAMS int kllitoa(long long int value, char* str, int base);
NON_NULL_PARAMS int kullitoa(unsigned long long int value, char* str, int base);

void _put_char(char c, int x, int y, unsigned char attrib);
void put_char(char c, int x, int y);
NON_NULL_PARAMS void dump_regs(const tcb_t* thread, const ExecutionState* state,
    unsigned int int_num, unsigned int error_code);
NON_NULL_PARAMS void dump_state(const ExecutionState* state,
    unsigned int int_num, unsigned int error_code);
void dump_stack(addr_t, addr_t);
static const char* DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

unsigned int sx = 0;
unsigned int sy = 1;

#define COM1 0x3f8
#define COM2 0x2f8

#define COMBASE COM1

void start_time_stamp(void)
{
    rdtsc(&upper1, &lower1);
}

void stop_time_stamp(void)
{
    rdtsc(&upper2, &lower2);
}

unsigned int get_time_difference(void)
{
    if(upper1 == upper2)
        return (unsigned)(lower2 - lower1);
    else if(upper2 == upper1 + 1) {
        if(lower2 < lower1)
            return (0xFFFFFFFFu - lower1) + lower2;
    }
    return 0xFFFFFFFFu;
}

/** Sets up the serial ports for printing debug messages
 (and possibly receiving commands).
 */
void init_serial(void)
{
    out_port8(COMBASE + 3, in_port8(COMBASE + 3) | 0x80u); // Set DLAB for
    // DLLB access
    out_port8(COMBASE, 0x03u);                             /* 38400 baud */
    out_port8(COMBASE + 1, 0u);                            // Disable interrupts
    out_port8(COMBASE + 3, in_port8(COMBASE + 3) & 0x7fu); // Clear DLAB
    out_port8(COMBASE + 3, 3u);                            // 8-N-1
}

int get_debug_char(void)
{
    while(!(in_port8(COMBASE + 5) & 0x01u))
        ;
    return in_port8(COMBASE);
}

void put_debug_char(int ch)
{
    out_port8(0xE9, (byte)ch); // Use E9 hack to output characters
    while(!(in_port8(COMBASE + 5) & 0x20u))
        ;
    out_port8(COMBASE, (byte)ch);
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

NON_NULL_PARAMS int kitoa(int value, char* str, int base)
{
    size_t strEnd = 0;
    int negative = base == 10 && value < 0;
    int charsWritten = 0;

    if(base < 2 || base > 36)
        return -1;

    if(negative) {
        if(value == INT_MIN) {
            memcpy(str, "-2147483648", 11);
            return 11;
        } else
            value = ~value + 1;
    }

    do {
        int quot = base * (value / base);

        str[strEnd++] = DIGITS[(size_t)(value - quot)];
        value = quot / base;
    } while(value);

    if(negative)
        str[strEnd++] = '-';

    charsWritten = (int)strEnd;

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

NON_NULL_PARAMS int kuitoa(unsigned int value, char* str, int base)
{
    size_t strEnd = 0;
    int charsWritten = 0;

    if(base < 2 || base > 36)
        return -1;

    do {
        unsigned int quot = (unsigned int)base * (value / (unsigned int)base);

        str[strEnd++] = DIGITS[(size_t)(value - quot)];
        value = quot / (unsigned int)base;
    } while(value);

    charsWritten = (int)strEnd;

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

NON_NULL_PARAMS int klitoa(long int value, char* str, int base)
{
    size_t strEnd = 0;
    int negative = base == 10 && value < 0;
    int charsWritten = 0;

    if(base < 2 || base > 36)
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
        } else
            value = ~value + 1;
    }

    do {
        long int quot = base * (value / base);

        str[strEnd++] = DIGITS[value - quot];
        value = quot / base;
    } while(value);

    if(negative)
        str[strEnd++] = '-';

    charsWritten = (int)strEnd;

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

NON_NULL_PARAMS int kulitoa(unsigned long int value, char* str, int base)
{
    size_t strEnd = 0;
    int charsWritten = 0;

    if(base < 2 || base > 36)
        return -1;

    do {
        unsigned long int quot = (unsigned long int)base * (value / (unsigned long int)base);

        str[strEnd++] = DIGITS[(size_t)(value - quot)];
        value = quot / (unsigned long int)base;
    } while(value);

    charsWritten = (int)strEnd;

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

NON_NULL_PARAMS int kllitoa(long long int value, char* str, int base)
{
    size_t strEnd = 0;
    int negative = base == 10 && value < 0;
    int charsWritten = 0;
    unsigned int v[2] = {
        (unsigned int)(value & 0xFFFFFFFFu),
        (unsigned int)(value >> 32) };

    if(base < 2 || base > 36)
        return -1;

    // Ensure base is a power of 2
    kassert((base & -base) == base);

    if(negative) {
        if(value == (long long int)LLONG_MIN) {
            memcpy(str, "-9223372036854775808", 20);
            return 20;
        } else
            value = ~value + 1;
    }

    for(int i = 0; i < 2; i++) {
        if(i == 1 && v[i] == 0)
            break;

        do {
            unsigned int quot = (unsigned int)base * (v[i] / (unsigned int)base);

            str[strEnd++] = DIGITS[v[i] - quot];
            v[i] = quot / (unsigned int)base;
        } while(v[i]);
    }

    if(negative)
        str[strEnd++] = '-';

    charsWritten = (int)strEnd;

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

NON_NULL_PARAMS int kullitoa(unsigned long long int value, char* str, int base)
{
    size_t str_end = 0;
    int chars_written = 0;
    unsigned int v[2] = {
        (unsigned int)(value & 0xFFFFFFFFu),
        (unsigned int)(value >> 32) };

    if(base < 2 || base > 36)
        return -1;

    for(int i = 0; i < 2; i++) {
        if(i == 1 && v[i] == 0)
            break;

        do {
            unsigned int quot = (unsigned int)base * (v[i] / (unsigned int)base);

            str[str_end++] = DIGITS[v[i] - quot];
            v[i] = quot / (unsigned int)base;
        } while(v[i]);
    }

    chars_written = (int)str_end;

    if(str_end) {
        str_end--;

        for(size_t i = 0; i < str_end; i++, str_end--) {
            char tmp = str[i];
            str[i] = str[str_end];
            str[str_end] = tmp;
        }
    }

    return chars_written;
}

void init_video(void)
{
    bad_assert_hlt = true; // false; // true;
    out_port8(MISC_OUTPUT_WR_REG, in_port8(MISC_OUTPUT_RD_REG) | FROM_FLAG_BIT(0));
}

/// Sets the flag to halt the system on a failed assertion

void set_bad_assert_hlt(bool value)
{
    bad_assert_hlt = value;
}

static void set_scroll(word addr)
{
    byte high = (addr >> 8) & 0xFFu;
    byte low = addr & 0xFFu;

    out_port8(CRT_CONTR_INDEX, START_ADDR_HIGH);
    out_port8(CRT_CONTR_DATA, high);
    out_port8(CRT_CONTR_INDEX, START_ADDR_LOW);
    out_port8(CRT_CONTR_DATA, low);
}

void reset_scroll(void)
{
    set_scroll(0);
}

static int scroll(int up)
{
    int ret = 0;
    byte high;
    byte low;
    word address;

    out_port8(CRT_CONTR_INDEX, START_ADDR_HIGH);
    high = in_port8(CRT_CONTR_DATA);
    out_port8(CRT_CONTR_INDEX, START_ADDR_LOW);
    low = in_port8(CRT_CONTR_DATA);
    address = (((word)high << 8) | low);

    if(up) {
        if(address == 0)
            address = CHARS_PER_LINE * MAX_LINES;
        else
            address -= CHARS_PER_LINE;
    } else {
        address += CHARS_PER_LINE;

        if(address > CHARS_PER_LINE * MAX_LINES) {
            address = 0;
            ret = 1;
        }
    }

    set_scroll(address);

    return ret;
}

void scroll_up(void)
{
    scroll(UP_DIR);
}

int scroll_down(void)
{
    return scroll(DOWN_DIR);
}

NON_NULL_PARAM(3)
void kprintf_at(unsigned int x, unsigned int y,
    const char* str, ...)
{
    unsigned int c_x = cursor_x;
    unsigned int c_y = cursor_y;
    va_list args;

    cursor_x = x;
    cursor_y = y;

    va_start(args, str);
    _kprintf(&print_char, str, args);
    va_end(args);

    cursor_x = c_x;
    cursor_y = c_y;
}

NON_NULL_PARAMS void print_assert_msg(const char* exp, const char* file,
    const char* func, int line)
{
    tcb_t* current_thread = get_current_thread();

    kprintf("\n<'%s' %s: %d> kassert(%s) failed\n", file, func, line, exp);

    if(current_thread) {
        dump_regs((tcb_t*)current_thread, &current_thread->user_exec_state, 0xFFFFFFFFu, 0xFFFFFFFFu);
    }

    if(bad_assert_hlt) {
        kprintf("\nSystem halted.");

        disable_int();
        asm("hlt");
    }
}

CALL_COUNTER(inc_sched_count)

/** Increments a counter that keeps track of the number of times that
 the scheduler was called.
 */
    void inc_sched_count(void)
{
    tcb_t* current_thread = get_current_thread();

    volatile word* vidmem = (volatile word*)(VIDMEM_START);

    word w = VGA_CHAR_AT(vidmem, 0, 0);

    VGA_CHAR_AT(vidmem, 0, 0) = VGA_CHAR(CHAR_OF(w + 1), RED, GRAY);

    kassert(current_thread != NULL);

    if(current_thread) {
        kprintf_at(2, 0, "tid: %5u cr3: %p", get_tid(current_thread),
            (void*)current_thread->root_pmap);
    }
}

/** Increments a counter that keeps track of the number of times that
 the timer interrupt was called.
 */
void inc_timer_count(void)
{
    volatile word* vidmem = (volatile word*)(VIDMEM_START);

    word w = VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1);

    VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1) = VGA_CHAR(CHAR_OF(w + 1), GREEN, GRAY);
}

/// Blanks the screen
void clear_screen(void)
{
    volatile word* vidmem = (volatile word*)(VIDMEM_START);

    for(int col = 0; col < SCREEN_WIDTH; col++)
        VGA_CHAR_AT(vidmem, 0, col) = VGA_CHAR(' ', BLACK, GRAY);

    for(int line = 1; line < SCREEN_HEIGHT; line++) {
        for(int col = 0; col < SCREEN_WIDTH; col++)
            VGA_CHAR_AT(vidmem, line, col) = VGA_CHAR(' ', GRAY, BLACK);
    }

    // memset( (char *)vidmem, 0, SCREEN_HEIGHT * SCREEN_WIDTH * 2 );

    reset_scroll();
}

NON_NULL_PARAMS void do_new_line(int* x, int* y)
{
    (*y)++;
    *x = 0;

    if(*y >= SCREEN_HEIGHT && scroll_down())
        *y = 0;
}

void print_char(int c)
{
    volatile word* vidmem = (volatile word*)VIDMEM_START + cursor_y * SCREEN_WIDTH + cursor_x;

    *vidmem = (*vidmem & 0xFF00) | (unsigned char)c;

    cursor_x++;

    if(cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
}

void _put_char(char c, int x, int y, unsigned char attrib)
{
    volatile word* vidmem = (volatile word*)(VIDMEM_START);

    VGA_CHAR_AT(vidmem, y, x) = VGA_CHAR(c, attrib & 0x0F, (attrib >> 4) & 0x0F);
}

void put_char(char c, int x, int y)
{
    _put_char(c, x, y, COLOR_ATTR(GRAY, BLACK));
}

void reset_printf_state(struct PrintfState* state)
{
    memset(state, 0, sizeof * state);
    state->precision = DEFAULT_PRECISION;
}

NON_NULL_PARAM(1)
void kprintf(const char* str, ...)
{
    va_list args;

    va_start(args, str);
    _kprintf(&put_debug_char, str, args);
    va_end(args);
}

/** A simplified stdio printf() for debugging use
 @param write_func The function to be used to output characters.
 @param str The formatted string to print with format specifiers
 @param ... The arguments to the specifiers
 */
NON_NULL_PARAMS void _kprintf(void (*write_func)(int), const char* str,
    va_list args)
{
    struct PrintfState state;
    char buf[KITOA_BUF_LEN];
    size_t i;
    int chars_written = 0;

    reset_printf_state(&state);

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
                    switch(state.printf_state) {
                        case FLAGS:
                        case WIDTH:
                            if(state.printf_state == FLAGS) {
                                if(str[i] == '0')
                                    state.is_zero_pad = 1;

                                state.printf_state = WIDTH;
                            }

                            state.width = 10 * state.width + (size_t)(str[i] - '0');
                            continue;
                        case PRECISION:
                            state.precision = (int)(10 * state.precision + (int)(str[i] - '0'));
                            continue;
                        case LENGTH:
                        default:
                            state.is_error = 1;
                            break;
                    }
                    break;
                case 'l':
                    if(state.printf_state == LENGTH) {
                        state.length = LONG_LONG_INT;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else if(state.printf_state < LENGTH) {
                        state.length = LONG_INT;
                        state.printf_state = LENGTH;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 'h':
                    if(state.printf_state == LENGTH) {
                        state.length = SHORT_INT;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else if(state.printf_state < LENGTH) {
                        state.length = CHAR;
                        state.printf_state = LENGTH;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 'j':
                    if(state.printf_state != SPECIFIER) {
                        state.length = INTMAX;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 'z':
                    if(state.printf_state != SPECIFIER) {
                        state.length = SIZE;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 't':
                    if(state.printf_state != SPECIFIER) {
                        state.length = PTRDIFF;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 'L':
                    if(state.printf_state != SPECIFIER) {
                        state.length = LONG_DOUBLE;
                        state.printf_state = SPECIFIER;
                        continue;
                    } else {
                        state.is_error = 1;
                        break;
                    }
                    break;
                case 'c':
                case '%':
                    if(str[i] == 'c') { /*
    if(state.length == LONG_INT)
    buf[0] = (char)va_arg(args, wint_t);
    else
    */
                        buf[0] = (char)va_arg(args, int);
                    } else
                        buf[0] = '%';

                    state.string = buf;
                    state.arg_length = 1;
                    break;
                case '.':
                    switch(state.printf_state) {
                        case FLAGS:
                        case WIDTH:
                            state.precision = 0;
                            state.printf_state = PRECISION;
                            continue;
                        default:
                            state.is_error = 1;
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
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned long long int), buf, 16);
                            break;
                        case CHAR:
                            state.arg_length = (size_t)itoa((unsigned char)va_arg(args, unsigned int), buf, 16);
                            break;
                        case SHORT_INT:
                            state.arg_length = (size_t)itoa((unsigned short int)va_arg(args, unsigned int), buf, 16);
                            break;
                        case LONG_INT:
                            state.arg_length = (size_t)itoa((unsigned long int)va_arg(args, unsigned long int), buf, 16);
                            break;
                        case DEFAULT:
                        default:
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned int), buf, 16);
                            break;
                    }

                    if(state.use_prefix) {
                        state.prefix[0] = '0';
                        state.prefix[1] = str[i] != 'X' ? 'x' : 'X';
                    }

                    if(str[i] == 'X') {
                        for(size_t pos = 0; pos < state.arg_length; pos++) {
                            if(buf[pos] >= 'a' && buf[pos] <= 'z')
                                buf[pos] -= 'a' - 'A';
                        }
                    }

                    state.string = buf;
                    break;
                case 'o':
                    switch(state.length) {
                        case LONG_LONG_INT:
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned long long int), buf, 8);
                            break;
                        case CHAR:
                            state.arg_length = (size_t)itoa((unsigned char)va_arg(args, unsigned int), buf, 8);
                            break;
                        case SHORT_INT:
                            state.arg_length = (size_t)itoa((unsigned short int)va_arg(args, unsigned int), buf, 8);
                            break;
                        case LONG_INT:
                            state.arg_length = (size_t)itoa((unsigned long int)va_arg(args, unsigned long int), buf, 8);
                            break;
                        case DEFAULT:
                        default:
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned int), buf, 8);
                            break;
                    }

                    if(state.use_prefix) {
                        state.prefix[0] = '0';
                        state.prefix[1] = '\0';
                    }

                    state.string = buf;
                    break;
                case 's':
                    state.string = va_arg(args, char*);

                    if(state.precision == DEFAULT_PRECISION)
                        for(state.arg_length = 0; state.string[state.arg_length];
                            state.arg_length++)
                            ;
                    else
                        state.arg_length = (size_t)state.precision;

                    break;
                case 'u':
                    switch(state.length) {
                        case LONG_LONG_INT:
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned long long int), buf, 10);
                            break;
                        case CHAR:
                            state.arg_length = (size_t)itoa((unsigned char)va_arg(args, unsigned int), buf, 10);
                            break;
                        case SHORT_INT:
                            state.arg_length = (size_t)itoa((unsigned short int)va_arg(args, unsigned int), buf, 10);
                            break;
                        case LONG_INT:
                            state.arg_length = (size_t)itoa((unsigned long int)va_arg(args, unsigned long int), buf, 10);
                            break;
                        case DEFAULT:
                        default:
                            state.arg_length = (size_t)itoa(va_arg(args, unsigned int), buf, 10);
                            break;
                    }

                    state.string = buf;
                    break;
                case 'd':
                case 'i':
                    switch(state.length) {
                        case LONG_LONG_INT:
                            state.arg_length = (size_t)itoa(va_arg(args, long long int), buf, 10);
                            break;
                        case CHAR:
                            state.arg_length = (size_t)itoa((signed char)va_arg(args, int), buf, 10);
                            break;
                        case SHORT_INT:
                            state.arg_length = (size_t)itoa((short int)va_arg(args, int), buf, 10);
                            break;
                        case LONG_INT:
                            state.arg_length = (size_t)itoa((long int)va_arg(args, long int), buf, 10);
                            break;
                        case DEFAULT:
                        default:
                            state.arg_length = (size_t)itoa(va_arg(args, int), buf, 10);
                            break;
                    }

                    state.is_negative = buf[0] == '-';
                    state.string = buf;

                    break;
                case '#':
                    if(state.printf_state == FLAGS) {
                        state.use_prefix = 1;
                        continue;
                    } else
                        state.is_error = 1;
                    break;
                case '-':
                    if(state.printf_state == FLAGS) {
                        state.is_left_justify = 1;
                        continue;
                    } else
                        state.is_error = 1;
                    break;
                case '+':
                    if(state.printf_state == FLAGS) {
                        state.is_forced_plus = 1;
                        continue;
                    } else
                        state.is_error = 1;
                    break;
                case ' ':
                    if(state.printf_state == FLAGS) {
                        state.is_blank_plus = 1;
                        continue;
                    } else
                        state.is_error = 1;
                    break;
                case '*':
                    switch(state.printf_state) {
                        case WIDTH:
                            state.width = (size_t)va_arg(args, int);
                            continue;
                        case PRECISION:
                            state.precision = va_arg(args, int);
                            continue;
                        default:
                            state.is_error = 1;
                            break;
                    }
                    break;
                case 'n':
                    if(state.printf_state == WIDTH || state.printf_state == PRECISION || state.is_forced_plus || state.is_blank_plus || state.is_zero_pad || state.use_prefix || state.is_left_justify) {
                        state.is_error = 1;
                        break;
                    } else {
                        switch(state.length) {
                            case LONG_LONG_INT:
                                *va_arg(args, long long int*) = (long long int)chars_written;
                                break;
                            case CHAR:
                                *va_arg(args, signed char*) = (signed char)chars_written;
                                break;
                            case SHORT_INT:
                                *va_arg(args, short int*) = (short int)chars_written;
                                break;
                            case LONG_INT:
                                *va_arg(args, long int*) = (long int)chars_written;
                                break;
                            case INTMAX:
                                *va_arg(args, intmax_t*) = (intmax_t)chars_written;
                                break;
                            case SIZE:
                                *va_arg(args, size_t*) = (size_t)chars_written;
                                break;
                            case PTRDIFF:
                                *va_arg(args, ptrdiff_t*) = (ptrdiff_t)chars_written;
                                break;
                            case DEFAULT:
                            default:
                                *va_arg(args, int*) = (int)chars_written;
                                break;
                        }
                    }
                    continue;
                default:
                    state.is_error = 1;
                    break;
            }

            if(state.is_error) {
                for(size_t j = state.percent_index; j <= i; j++, chars_written++)
                    write_func(str[j]);
            } else {
                size_t arg_length = state.arg_length - (state.is_negative ? 1 : 0);
                size_t sign_length =
                    (state.is_negative || state.is_forced_plus || state.is_blank_plus) ? 1 : 0;
                size_t prefix_length;
                size_t zero_pad_length;
                size_t space_pad_length;

                if(state.use_prefix)
                    prefix_length =
                    state.prefix[0] == '\0' ? 0 : (state.prefix[1] == '\0' ? 1 : 2);
                else
                    prefix_length = 0;

                if(str[i] == 's' || state.precision == DEFAULT_PRECISION || arg_length > INT_MAX || (int)arg_length >= state.precision) {
                    zero_pad_length = 0;
                } else {
                    zero_pad_length = (size_t)state.precision - arg_length;
                }

                if(arg_length + sign_length + prefix_length + zero_pad_length < state.width)
                    space_pad_length = state.width - arg_length - sign_length - prefix_length - zero_pad_length;
                else
                    space_pad_length = 0;

                if(!state.is_left_justify) {
                    for(; space_pad_length > 0; space_pad_length--, chars_written++)
                        write_func(' ');
                }

                if(state.is_negative) {
                    write_func('-');
                    chars_written++;
                } else if(state.is_forced_plus) {
                    write_func('+');
                    chars_written++;
                } else if(state.is_blank_plus) {
                    write_func(' ');
                    chars_written++;
                }

                for(size_t pos = 0; pos < prefix_length; pos++, chars_written++)
                    write_func(state.prefix[pos]);

                for(; zero_pad_length > 0; zero_pad_length--, chars_written++)
                    write_func('0');

                for(size_t pos = (state.is_negative ? 1 : 0); arg_length > 0;
                    arg_length--, pos++, chars_written++)
                    write_func(state.string[pos]);

                if(state.is_left_justify) {
                    for(; space_pad_length > 0; space_pad_length--, chars_written++)
                        write_func(' ');
                }
            }
            reset_printf_state(&state);
        } else {
            switch(str[i]) {
                case '%':
                    state.percent_index = i;
                    state.inPercent = 1;
                    memset(buf, 0, KITOA_BUF_LEN);
                    break;
                case '\n':
                    write_func('\r');
                    write_func('\n');
                    chars_written += 2;
                    break;
                case '\r':
                    write_func('\r');
                    chars_written++;
                    break;
                default:
                    write_func(str[i]);
                    chars_written++;
                    break;
            }
        }
    }
}

NON_NULL_PARAMS void dump_state(const ExecutionState* exec_state,
    unsigned int int_num, unsigned int error_code)
{
    if(int_num < IRQ_BASE)
        kprintf("Exception %u", int_num);
    else
        kprintf("IRQ%u", int_num - IRQ_BASE);

    kprintf("\nEAX: 0x%lx EBX: 0x%lx ECX: 0x%lx EDX: 0x%lx EFLAGS: 0x%lx",
        exec_state->eax, exec_state->ebx, exec_state->ecx, exec_state->edx,
        exec_state->eflags);
    kprintf(
        "\nESI: 0x%lx EDI: 0x%lx EBP: 0x%lx ESP: 0x%lx EIP: 0x%lx\n",
        exec_state->esi,
        exec_state->edi,
        exec_state->ebp,
        exec_state->cs == UCODE_SEL ? exec_state->user_esp : (uint32_t)(exec_state + 1) - 2 * sizeof(uint32_t),
        exec_state->eip);
    kprintf(
        "\nCS: 0x%x DS: 0x%x ES: 0x%x FS: 0x%x GS: 0x%x SS: 0x%x\n",
        exec_state->cs, exec_state->ds, exec_state->es, exec_state->fs, exec_state->gs,
        exec_state->cs == UCODE_SEL ? exec_state->user_ss : get_ss());

    kprintf("CR0: 0x%lx CR2: 0x%lx CR3: 0x%lx CR4: 0x%lx", get_cr0(), get_cr2(),
        get_cr3(), get_cr4());

    if(int_num == 8 || (int_num >= 10 && int_num <= 14) || int_num == 17 || int_num == 21) {
        kprintf(" error code: 0x%x\n", error_code);
    }
}

void dump_stack(addr_t stack_frame_ptr, addr_t addr_space)
{
    kprintf("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*\n");

    while(stack_frame_ptr) {
        kprintf("<0x%lx>:", stack_frame_ptr);

        if(is_readable(stack_frame_ptr + sizeof(dword), addr_space))
            kprintf(" [0x%lx]", *(dword*)(stack_frame_ptr + sizeof(dword)));
        else
            kprintf(" [???]");

        for(size_t i = 2; i < 8; i++) {
            if(is_readable((addr_t)(stack_frame_ptr + sizeof(dword) * i), addr_space))
                kprintf(" 0x%lx", *(dword*)(stack_frame_ptr + sizeof(dword) * i));
            else
                break;
        }

        kprintf("\n");

        if(!is_readable((addr_t) * (dword*)stack_frame_ptr, addr_space)) {
            kprintf("<0x%lx (invalid)>:\n", *(dword*)stack_frame_ptr);
            break;
        } else
            stack_frame_ptr = *(dword*)stack_frame_ptr;
    }
}

/** Prints useful debugging information about the current thread
 @param thread The current thread
 @param exec_state The saved execution state that was pushed to the stack upon context switch
 @param int_num The interrupt vector (if applicable)
 @param error_code The error code provided by the processor (if applicable)
 */
NON_NULL_PARAMS void dump_regs(const tcb_t* thread,
    const ExecutionState* exec_state,
    unsigned int int_num, unsigned int error_code)
{
    kprintf("Thread: %p (TID: %u) ", thread, get_tid(thread));

    dump_state(exec_state, int_num, error_code);

    kprintf("Thread CR3: %#lx Current CR3: %#lx\n",
        *(const uint32_t*)&thread->root_pmap, get_cr3());

    dump_stack((addr_t)exec_state->ebp, (addr_t)thread->root_pmap);
}

#endif /* DEBUG */

noreturn void abort(void);

noreturn void abort(void)
{
    addr_t stack_frame_ptr;

    kprintf("Debug Error: abort() has been called.\n");
    __asm__("mov %%ebp, %0\n" : "=m"(stack_frame_ptr));
    dump_stack(stack_frame_ptr, get_root_page_map());

    while(1) {
        disable_int();
        halt();
    }
}

NON_NULL_PARAMS noreturn void print_panic_msg(const char* msg, const char* file, const char* func,
    int line)
{
    addr_t stack_frame_ptr;
    kprintf("\nKernel panic - %s(): %d (%s). %s\nSystem halted\n", func, line,
        file, msg);

    __asm__("mov %%ebp, %0\n" : "=m"(stack_frame_ptr));
    dump_stack(stack_frame_ptr, get_root_page_map());

    while(1) {
        disable_int();
        halt();
    }
}
