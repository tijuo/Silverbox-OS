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
#include <kernel/lock.h>

#ifdef DEBUG

extern addr_t kboot_stack_top;
extern paddr_t kpage_dir;

// TODO: This doesn't actually lock the VGA VRAM, itself

#define VIDMEM_START PHYS_TO_VIRT(0xB8000u)

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
#define VGA_CHAR(c, fg, bg) (uint16_t)((c) | COLOR_ATTR(fg, bg) << 8)

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

typedef struct {
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
    unsigned int width;
    int precision;
    size_t percent_index;
    size_t arg_length;
    char* string;
} PrintfState;

typedef struct {
    unsigned int x;
    unsigned int y;
} VideoCursor;
typedef struct {
    unsigned int x;
    unsigned int y;
} SerialCursor;



LOCKED_WITH(VideoCursor, video_cursor, ((VideoCursor){.x = 0, .y = 0}))
LOCKED_WITH(SerialCursor, serial_cursor, ((SerialCursor){.x = 0, .y = 0 }))

NON_NULL_PARAMS void reset_printf_state(PrintfState* state);

static void print_char(int c);
NON_NULL_PARAMS static void _kprintf(void (*write_func)(int), const char* str,
    va_list args);
NON_NULL_PARAM(1)
void kprintf(const char* str, ...) __attribute__((format(printf, 1, 2)));
NON_NULL_PARAM(3)
void kprintf_at(unsigned int x, unsigned int y,
    const char* str, ...) __attribute__((format(printf, 3, 4)));
static int scroll(int up);
static void set_scroll(uint16_t addr);
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
void dump_stack(addr_t, addr_t);
const char* const DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";
const char* const EXCEPTION_NAMES[32] = {
    "Divide Error",
    "Debug Exception",
    "NMI Interrupt",
    "Breakpoint Exception",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Exception",
    "Page Fault",
    "Unknown Exception",
    "x87 FPU Floating-Point Error",
    "Alignment Check",
    "Machine-Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception",
    "Unknown Exception"
};

#define COM1 0x3f8
#define COM2 0x2f8

#define COMBASE COM1

/* Note: RDTSCP should be used instead due to possible out of order execution.
 * Alternatively, CPUID or MFENCE,LFENCE can be used before RDTSC
 */
#define RDTSC(state, out_val)   __asm__ __volatile__( "rdtsc" : "=a"( state[0] ), "=d"( state[1] ) )
#define CALC_TIME(func_call) do {\
    uint32_t start_state[2] = { 0, 0 };\
    uint32_t stop_state[2] = { 0, 0 };\
    RDTSC(start_state);\
    func_call;\
    RDTSC(stop_state);\
    out_val = get_time_difference(&start_state, &stop_state);\
} while(0)

uint64_t get_time_difference(uint32_t(*start_state)[2], uint32_t(*stop_state)[2])
{
    uint64_t* start = (uint64_t*)start_state;
    uint64_t* stop = (uint64_t*)stop_state;

    return *stop - *start;
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

// Assumes serial device locked
int get_debug_char(void)
{
    while(!(in_port8(COMBASE + 5) & 0x01u))
        ;
    return in_port8(COMBASE);
}

// Locks serial cursor
// Assumes video cursor is locked
// Assumes video registers are locked
void put_debug_char(int ch)
{
    SPINLOCK_ACQUIRE(serial_cursor)
    SerialCursor c = LOCK_VAL(serial_cursor);

    out_port8(0xE9, (uint8_t)ch); // Use E9 hack to output characters
    while(!(in_port8(COMBASE + 5) & 0x20u))
        ;
    out_port8(COMBASE, (uint8_t)ch);

    if(ch == '\r') {
        c.x = 0;
        c.y++;
        if(c.y >= SCREEN_HEIGHT)
            scroll_down();
    } else if(ch == '\t') {
        c.x += 8;
    } else {
        put_char(ch, c.x, c.y);
        c.x++;
    }

    if(c.x >= SCREEN_WIDTH) {
        c.x = 0;
        c.y++;
        if(c.y >= SCREEN_HEIGHT)
            scroll_down();
    }


    LOCK_SET(serial_cursor, c);
    SPINLOCK_RELEASE(serial_cursor);
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
    KASSERT((base & -base) == base);

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
    out_port8(MISC_OUTPUT_WR_REG, in_port8(MISC_OUTPUT_RD_REG) | FROM_FLAG_BIT(0));
}

// Assumes video registers are locked
static void set_scroll(uint16_t addr)
{
    uint8_t high = (addr >> 8) & 0xFFu;
    uint8_t low = addr & 0xFFu;

    out_port8(CRT_CONTR_INDEX, START_ADDR_HIGH);
    out_port8(CRT_CONTR_DATA, high);
    out_port8(CRT_CONTR_INDEX, START_ADDR_LOW);
    out_port8(CRT_CONTR_DATA, low);
}

// Assumes video registers are locked
void reset_scroll(void)
{
    set_scroll(0);
}

// Assumes video registers are locked
static int scroll(int up)
{
    int ret = 0;
    uint8_t high;
    uint8_t low;
    uint16_t address;

    out_port8(CRT_CONTR_INDEX, START_ADDR_HIGH);
    high = in_port8(CRT_CONTR_DATA);
    out_port8(CRT_CONTR_INDEX, START_ADDR_LOW);
    low = in_port8(CRT_CONTR_DATA);
    address = (((uint16_t)high << 8) | low);

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

// Assumes video registers are locked
void scroll_up(void)
{
    scroll(UP_DIR);
}

// Assumes video registers are locked
int scroll_down(void)
{
    return scroll(DOWN_DIR);
}

NON_NULL_PARAM(3)
void kprintf_at(unsigned int x, unsigned int y,
    const char* str, ...)
{
    SPINLOCK_ACQUIRE(video_cursor);
    VideoCursor saved_cursor = LOCK_VAL(video_cursor);

    va_list args;

    LOCK_SET(video_cursor, ((VideoCursor){.x = x, .y = y }));
    SPINLOCK_RELEASE(video_cursor);

    va_start(args, str);
    _kprintf(&print_char, str, args);
    va_end(args);

    SPINLOCK_ACQUIRE(video_cursor);
    LOCK_SET(video_cursor, saved_cursor);
    SPINLOCK_RELEASE(video_cursor);
}

NON_NULL_PARAMS void print_assert_msg(const char* exp, const char* file,
    const char* func, int line)
{
    tcb_t* current_thread = thread_get_current();

    kprintfln("\n<'%s' %s: %d> KASSERT(%s) failed", file, func, line, exp);

    if(current_thread) {
        dump_regs((tcb_t*)current_thread, &current_thread->user_exec_state, 0xFFFFFFFFu, 0xFFFFFFFFu);
    }

    if(BAD_ASSERT_HLT) {
        kprintf("System halted.");

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
    tcb_t* current_thread = thread_get_current();

    volatile uint16_t* vidmem = (volatile uint16_t*)(VIDMEM_START);

    uint16_t w = VGA_CHAR_AT(vidmem, 0, 0);

    VGA_CHAR_AT(vidmem, 0, 0) = VGA_CHAR(CHAR_OF(w + 1), RED, GRAY);

    KASSERT(current_thread != NULL);

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
    volatile uint16_t* vidmem = (volatile uint16_t*)(VIDMEM_START);

    uint16_t w = VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1);

    VGA_CHAR_AT(vidmem, 0, SCREEN_WIDTH - 1) = VGA_CHAR(CHAR_OF(w + 1), GREEN, GRAY);
}

/// Blanks the screen
void clear_screen(void)
{
    volatile uint16_t* vidmem = (volatile uint16_t*)(VIDMEM_START);

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
    // Do nothing if char isn't an ASCII printable character.
    if(c < 32 || c > 126)
        return;

    SPINLOCK_ACQUIRE(video_cursor);
    VideoCursor cursor = LOCK_VAL(video_cursor);

    volatile uint16_t* vidmem = (volatile uint16_t*)VIDMEM_START + cursor.y * SCREEN_WIDTH + cursor.x;

    *vidmem = (*vidmem & 0xFF00) | (unsigned char)c;

    cursor.x++;

    if(cursor.x >= SCREEN_WIDTH) {
        cursor.x = 0;
        cursor.y++;
    }

    LOCK_SET(video_cursor, cursor);

    SPINLOCK_RELEASE(video_cursor);
}

// Assumes that VRAM is already locked.
void _put_char(char c, int x, int y, unsigned char attrib)
{
    volatile uint16_t* vidmem = (volatile uint16_t*)(VIDMEM_START);

    VGA_CHAR_AT(vidmem, y, x) = VGA_CHAR(c, attrib & 0x0F, (attrib >> 4) & 0x0F);
}

// Assumes that VRAM is already locked.
void put_char(char c, int x, int y)
{
    _put_char(c, x, y, COLOR_ATTR(GRAY, BLACK));
}

void reset_printf_state(PrintfState* state)
{
    memset(state, 0, sizeof * state);
    state->precision = DEFAULT_PRECISION;
}

// Locks/releases video cursor
NON_NULL_PARAM(1)
void kprintf(const char* str, ...)
{
    va_list args;

    va_start(args, str);
    SPINLOCK_ACQUIRE(video_cursor);
    _kprintf(&put_debug_char, str, args);
    SPINLOCK_RELEASE(video_cursor);
    va_end(args);
}

// Locks/releases video cursor
NON_NULL_PARAM(1)
void kprintfln(const char* str, ...)
{
    va_list args;

    va_start(args, str);
    SPINLOCK_ACQUIRE(video_cursor);
    _kprintf(&put_debug_char, str, args);
    va_end(args);

    va_start(args, str);
    _kprintf(&put_debug_char, "\n", args);
    SPINLOCK_RELEASE(video_cursor);
    va_end(args);
}

/** A simplified stdio printf() for debugging use.
 * 
 @param write_func The function to be used to output characters.
 @param str The formatted string to print with format specifiers
 @param ... The arguments to the specifiers
 */
NON_NULL_PARAMS void _kprintf(void (*write_func)(int), const char* str,
    va_list args)
{
    PrintfState state;
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
        kprintfln("%s (exception %lu)", EXCEPTION_NAMES[int_num], int_num);
    else if(int_num >= IRQ_BASE && int_num < IRQ(24))
        kprintfln("IRQ %lu", int_num - IRQ_BASE);
    else if(int_num < 256)
        kprintfln("Software Interrupt %lu", int_num);
    else
        kprintfln("Invalid Interrupt %lu", int_num);

    kprintfln("EAX: %#.08lx EBX: %#.08lx ECX: %#.08lx EDX: %#.08lx EFL: %#.08lx",
        exec_state->eax, exec_state->ebx, exec_state->ecx, exec_state->edx,
        exec_state->eflags);
    kprintfln(
        "ESI: %#.08lx EDI: %#.08lx EBP: %#.08lx ESP: %#.08lx EIP: %#.08lx",
        exec_state->esi,
        exec_state->edi,
        exec_state->ebp,
        exec_state->cs == UCODE_SEL ? exec_state->user_esp : (uint32_t)(exec_state + 1) - 2 * sizeof(uint32_t),
        exec_state->eip);
    kprintfln(
        "CS:  %#.04x DS: %#.04x ES: %#.04x FS: %#.04x GS: %#.04x SS: %#.04x",
        exec_state->cs, exec_state->ds, exec_state->es, exec_state->fs, exec_state->gs,
        exec_state->cs == UCODE_SEL ? exec_state->user_ss : get_ss());

    kprintf("CR0: %#.08lx CR2: %#.08lx CR3: %#.08lx CR4: %#.08lx", get_cr0(), get_cr2(),
        get_cr3(), get_cr4());

    if(int_num == 8 || (int_num >= 10 && int_num <= 14) || int_num == 17 || int_num == 21) {
        kprintfln(" error code: %#.08lx", error_code);
    } else {
        kprintfln("");
    }

    dump_stack((addr_t)exec_state->ebp, get_cr3());
}

static bool is_stack_top(addr_t stack_frame_ptr, paddr_t addr_space)
{
    return (addr_space == (addr_t)&kpage_dir && (addr_t)stack_frame_ptr == (addr_t)&kboot_stack_top) || stack_frame_ptr == (addr_t)kernel_stack_top;
}

void dump_stack(addr_t stack_frame_ptr, paddr_t addr_space)
{
    kprintfln("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*");

    while(stack_frame_ptr) {
        bool is_at_top = is_stack_top(stack_frame_ptr, addr_space);

        kprintf("<%#.08lx>:%s", stack_frame_ptr, is_at_top ? " TOP" : "");

        if(is_at_top) {
            break;
        }

        if(is_readable(stack_frame_ptr, addr_space) != 1) {
            kprintfln(" unable to read frame data.");
            break;
        }

        for(size_t i = 1; i < 8; i++) {
            if(is_stack_top(stack_frame_ptr + i * sizeof(uint32_t), addr_space)) {
                break;
            }

            if(i == 1) {
                if(is_readable(stack_frame_ptr + sizeof(uint32_t), addr_space) == 1)
                    kprintf(" [%#.08lx]", *(uint32_t*)(stack_frame_ptr + sizeof(uint32_t)));
                else
                    kprintf(" [???]");
            } else if(is_readable((addr_t)(stack_frame_ptr + sizeof(uint32_t) * i), addr_space) == 1 && (void*)(stack_frame_ptr + sizeof(uint32_t) * i) != NULL) {
                kprintf(" %#.08lx", *(uint32_t*)(stack_frame_ptr + sizeof(uint32_t) * i));
            } else {
                kprintf(" ???");
            }
        }

        kprintfln("");

        stack_frame_ptr = *(uint32_t*)stack_frame_ptr;
    }
}

/** Prints useful debugging information about the current thread
 @param thread The current thread
 @param exec_state The saved execution state that was pushed to the stack upon context switch
 @param int_num The interrupt vector (if applicable)
 @param error_code The error code provided by the processor (if applicable)
 */
NON_NULL_PARAM(2) void dump_regs(const tcb_t* thread,
    const ExecutionState* exec_state,
    unsigned int int_num, unsigned int error_code)
{
    if(thread) {
        kprintf("Thread: %p (TID: %u) ", thread, get_tid(thread));
    }

    dump_state(exec_state, int_num, error_code);

    if(thread) {
        kprintfln("Thread CR3: %#lx Current CR3: %#lx",
            *(const uint32_t*)&thread->root_pmap, get_cr3());
    }
}

#endif /* DEBUG */

noreturn void abort(void);

noreturn void abort(void)
{
    addr_t stack_frame_ptr;

    kprintfln("Debug Error: abort() has been called.");
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
    kprintfln("\nKernel panic - %s(): %d (%s). %s\nSystem halted.", func, line,
        file, msg);

    __asm__("mov %%ebp, %0\n" : "=m"(stack_frame_ptr));
    dump_stack(stack_frame_ptr, get_root_page_map());

    while(1) {
        disable_int();
        halt();
    }
}
