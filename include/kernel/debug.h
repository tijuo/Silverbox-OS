#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>
#include <kernel/thread.h>

static inline noreturn void unreachable(void) {
    __asm__("ud2");
}

#ifdef DEBUG

#define BAD_ASSERT_HLT          1

#define PRINT_DEBUG(msg, ...)      do {\
kprintf("<'%s' %s: %d> ", __FILE__, __func__, __LINE__);\
kprintfln(msg, ##__VA_ARGS__);\
} while(0)

#define RET_MSG(ret, msg, ...)     do {\
kprintf("<'%s' %s: line %d> Returned: %d. ", __FILE__, __func__, __LINE__, (ret));\
kprintfln(msg, ##__VA_ARGS__);\
return ret;\
} while(0)

#define DECL_CALL_COUNTER(fname)		extern unsigned int fname ## _counter;
#define CALL_COUNTER(fname)			    unsigned int fname ## _counter;
#define INC_COUNT()				        __func__ ##_counter++

#define BOCHS_BREAKPOINT		        asm volatile("xchgw %bx, %bx\n")

void init_serial(void);
int get_debug_char(void);
void put_debug_char(int ch);

DECL_CALL_COUNTER(inc_sched_count)
void inc_sched_count(void);
void inc_timer_count(void);
void clear_screen(void);
NON_NULL_PARAM(1) void kprintf(const char* str, ...);
NON_NULL_PARAM(1) void kprintfln(const char *str, ...);

NON_NULL_PARAMS void print_assert_msg(const char* exp, const char* file,
    const char* func, int line);
void set_video_low_mem(bool value);
NON_NULL_PARAM(2) void dump_regs(const tcb_t* thread, const ExecutionState* state,
    unsigned int int_num, unsigned int error_code);
NON_NULL_PARAMS void dump_state(const ExecutionState* state,
    unsigned int int_num, unsigned int error_code);

NON_NULL_PARAMS void print_string(const char*, ...);
void init_video(void);


WARN_UNUSED uint64_t get_time_difference(uint32_t (*start_state)[2], uint32_t (*stop_state)[2]);

// Note: If `exp` contains functions, they should not have side-effects. This is because
// KASSERT() in release builds do nothing
#define KASSERT(exp)  do {\
  if(!(exp)){\
    BOCHS_BREAKPOINT;\
    print_assert_msg( #exp, __FILE__, __func__, __LINE__ );\
  }\
} while(0)

#else

#define KNOP                                ({})
#define KASSERT(exp)                        KNOP
#define inc_sched_count()                   KNOP
#define inc_timer_count()                   KNOP
#define clear_screen()                      KNOP
#define kprintf( ... )                      KNOP
#define kprintfln( ... )                    KNOP
#define print_assert_msg( w, x, y, z )      KNOP
#define set_video_low_mem( val )            KNOP
#define dump_regs( t, s, i, e )             KNOP
#define dump_state( s, i, e )               KNOP
#define dump_stack( x, y )                  KNOP

#define get_time_difference()               KNOP
//#define kprintInt( num )
//#define kprintHex( num )
//#define kprint3Nums( str, ... )
//#define putChar(c, i, j)
//#define _putChar(c, i, j, attr)
//#define printString(str, x, y)
//#define _printString(str, x, y, ...)
#define init_video()                        KNOP
#define PRINT_DEBUG(msg, ...)                    KNOP
#define RET_MSG(x, y, ...)	                    return (x)
#define CALL_COUNTER(ret_type, fname, arg)	ret_type fname(arg);
#define BOCHS_BREAKPOINT
#define DECL_CALL_COUNTER(fname)		    
#define INC_COUNT()				            
#endif /* DEBUG */

NON_NULL_PARAMS noreturn void print_panic_msg(const char* msg, const char* file, const char* func, int line);

#define PANIC(msg)     print_panic_msg( msg, __FILE__, __func__, __LINE__ )
#define UNREACHABLE     do { PANIC("An unreachable section of code has been reached."); unreachable(); } while(0)
#endif /* DEBUG_H */