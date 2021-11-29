#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>
#include <kernel/thread.h>

#ifdef DEBUG

#define PRINT_DEBUG(msg)      ({ kprintf("<'%s' %s: %d> %s", \
                              __FILE__, __func__, __LINE__, (msg)); })
#define RET_MSG(ret, msg)     ({ kprintf("<'%s' %s: line %d> Returned: %d. %s\n", \
				              __FILE__, __func__, __LINE__, (ret), (msg)); \
				              return ret; })

#define DECL_CALL_COUNTER(fname)		extern unsigned int fname ## _counter;
#define CALL_COUNTER(fname)			unsigned int fname ## _counter;
#define INC_COUNT()				__func__ _counter++

#define BOCHS_BREAKPOINT		asm volatile("xchgw %bx, %bx\n")

void init_serial(void);
int get_debug_char(void);
void put_debug_char(int ch);

DECL_CALL_COUNTER(inc_sched_count)
void inc_sched_count(void);
void inc_timer_count(void);
void clear_screen(void);
NON_NULL_PARAM(1) void kprintf(const char *str, ...);
NON_NULL_PARAMS void print_assert_msg(const char *exp, const char *file,
                                    const char *func, int line);
void set_bad_assert_hlt( bool value);
void set_video_low_mem( bool value);
NON_NULL_PARAMS void dump_regs(const tcb_t *thread, const ExecutionState *state,
                               unsigned int int_num, unsigned int error_code);
NON_NULL_PARAMS void dump_state(const ExecutionState *state,
                                unsigned int int_num, unsigned int error_code);

/*
 char *_toHexString(unsigned int num);
 char *__toHexString(unsigned int, int);
 char *_toIntString(int num);
 char *__toIntString(int, int);
 void putChar( char, int, int );
 void _putChar( char, int, int, unsigned char );
 */
NON_NULL_PARAMS void print_string(const char*, ...);
void init_video(void);

/* Note: RDTSCP should be used instead due to possible out of order execution.
 * Alternatively, CPUID or MFENCE,LFENCE can be used before RDTSC
 */

#define rdtsc( upper, lower ) asm __volatile__( "rdtsc\n" : "=a"( *lower ), "=d"( *upper ) )

void start_time_stamp(void);
void stop_time_stamp(void);
unsigned int get_time_difference(void);

#define calc_time(function, ret) \
  start_time_stamp(); \
  function; \
  stop_time_stamp();\
  ret = get_time_difference();

#define assert(exp)  { \
	__typeof__ (exp) _exp=(exp); \
  if(_exp) { \
  } else { \
  	BOCHS_BREAKPOINT; \
    print_assert_msg( #exp, __FILE__, __func__, __LINE__ ); \
  } \
}

#else

#define assert(exp)         ({})
#define inc_sched_count()     ({})
#define inc_timer_count()     ({})
#define clear_screen()       ({})
#define kprintf( ... )   ({})
#define print_assert_msg( w, x, y, z )    ({})
#define set_bad_assert_hlt( val )      ({})
#define set_video_low_mem( val )       ({})
#define dump_regs( t, s, i, e )     ({})
#define dump_state( s, i, e )       ({})
#define dump_stack( x, y )          ({})

//#define kprintInt( num )
//#define kprintHex( num )
#define calc_time(func, ret) func
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
