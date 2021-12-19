#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>
#include <kernel/thread.h>

#ifdef DEBUG

#define PRINT_DEBUG(msg)      kprintf("<'%s' %s: %d> %s", \
                              __FILE__, __func__, __LINE__, (msg))
#define RET_MSG(ret, msg)     return ({\
  __typeof__(ret) _ret=(ret);\
  kprintf("<'%s' %s: line %d> Returned: %d. %s\n",\
    __FILE__, __func__, __LINE__, (_ret), (msg));\
  _ret;\
})

#define DECL_CALL_COUNTER(fname)		extern unsigned long int fname ## _counter;
#define CALL_COUNTER(fname)			unsigned long int fname ## _counter;
#define INC_COUNT()				__func__ _counter++

#define BOCHS_BREAKPOINT		asm volatile("xchgw %bx, %bx\n")

void init_serial(void);
int get_debug_char(void);
void put_debug_char(int ch);

void clear_screen(void);
NON_NULL_PARAM(1) void kprintf(const char *str, ...);
NON_NULL_PARAMS void print_assert_msg(const char *exp, const char *file,
                                    const char *func, int line);
void set_bad_assert_hlt( bool value);
void set_video_low_mem( bool value);
NON_NULL_PARAMS void dump_regs(const tcb_t *thread, const exec_state_t *state,
                               unsigned long int int_num, unsigned long int error_code);
NON_NULL_PARAMS void dump_state(const exec_state_t *state,
                                unsigned long int int_num, unsigned long int error_code);

NON_NULL_PARAMS void print_string(const char*, ...);
void init_video(void);

/* Note: RDTSCP should be used instead due to possible out of order execution.
 * Alternatively, CPUID or MFENCE,LFENCE can be used before RDTSC
 */

#define rdtsc( upper, lower ) asm __volatile__( "rdtsc\n" : "=a"( *lower ), "=d"( *upper ) )

#define kassert(exp)  { \
	__typeof__ (exp) _exp=(exp); \
  if(_exp) { \
  } else { \
  	BOCHS_BREAKPOINT; \
    print_assert_msg( #exp, __FILE__, __func__, __LINE__ ); \
  } \
}

#else

#define kassert(exp)         ({})
#define clear_screen()       ({})
#define kprintf( ... )   ({})
#define print_assert_msg( w, x, y, z )    ({})
#define set_bad_assert_hlt( val )      ({})
#define set_video_low_mem( val )       ({})
#define dump_regs( t, s, i, e )     ({})
#define dump_state( s, i, e )       ({})

//#define kprintInt( num )
//#define kprintHex( num )
//#define kprint3Nums( str, ... )
//#define putChar(c, i, j)
//#define _putChar(c, i, j, attr)
//#define printString(str, x, y)
//#define _printString(str, x, y, ...)
#define init_video()                 ({})
#define PRINT_DEBUG(msg)            ({})
#define RET_MSG(x, y)	return (x)
#define CALL_COUNTER(ret_type, fname, arg)	ret_type fname(arg);
#define BOCHS_BREAKPOINT
#endif /* DEBUG */

NON_NULL_PARAMS noreturn void print_panic_msg(const char *msg, const char *file, const char *func, int line);

#define panic(msg)     print_panic_msg( msg, __FILE__, __func__, __LINE__ )

#endif /* DEBUG_H */
