#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>

#ifdef DEBUG

#include <kernel/mm.h>
#include <kernel/thread.h>
#define  VIDMEM_START   KPHYS_TO_VIRT(0xB8000u)

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
int getDebugChar(void);
void putDebugChar(int ch);

DECL_CALL_COUNTER(incSchedCount)
void incSchedCount(void);
void incTimerCount(void);
void clearScreen(void);
NON_NULL_PARAM(1) void kprintf(const char *str, ...);
NON_NULL_PARAMS void printAssertMsg(const char *exp, const char *file,
                                    const char *func, int line);
void setBadAssertHlt( bool value);
void setVideoLowMem( bool value);
NON_NULL_PARAMS void dump_regs(const tcb_t *thread, const ExecutionState *state,
                               unsigned int intNum, unsigned int errorCode);
NON_NULL_PARAMS void dump_state(const ExecutionState *state,
                                unsigned int intNum, unsigned int errorCode);

/*
 char *_toHexString(unsigned int num);
 char *__toHexString(unsigned int, int);
 char *_toIntString(int num);
 char *__toIntString(int, int);
 void putChar( char, int, int );
 void _putChar( char, int, int, unsigned char );
 */
NON_NULL_PARAMS void printString(const char*, ...);
void initVideo(void);

/* Note: RDTSCP should be used instead due to possible out of order execution.
 * Alternatively, CPUID or MFENCE,LFENCE can be used before RDTSC
 */

#define rdtsc( upper, lower ) asm __volatile__( "rdtsc\n" : "=a"( *lower ), "=d"( *upper ) )

void startTimeStamp(void);
void stopTimeStamp(void);
unsigned int getTimeDifference(void);

#define calcTime(function, ret) \
  startTimeStamp(); \
  function; \
  stopTimeStamp();\
  ret = getTimeDifference();

#define assert(exp)  { \
	__typeof__ (exp) _exp=(exp); \
  if(_exp) { \
  } else { \
  	BOCHS_BREAKPOINT; \
    printAssertMsg( #exp, __FILE__, __func__, __LINE__ ); \
  } \
}

#else

#define assert(exp)         ({})
#define incSchedCount()     ({})
#define incTimerCount()     ({})
#define clearScreen()       ({})
#define kprintf( ... )   ({})
#define printAssertMsg( w, x, y, z )    ({})
#define setBadAssertHlt( val )      ({})
#define setVideoLowMem( val )       ({})
#define dump_regs( t, s, i, e )     ({})
#define dump_state( s, i, e )       ({})
#define dump_stack( x, y )          ({})

//#define kprintInt( num )
//#define kprintHex( num )
#define calcTime(func, ret) func
//#define kprint3Nums( str, ... )
//#define putChar(c, i, j)
//#define _putChar(c, i, j, attr)
//#define printString(str, x, y)
//#define _printString(str, x, y, ...)
#define initVideo()                 ({})
#define PRINT_DEBUG(msg)            ({})
#define RET_MSG(x, y)	return (x)
#define CALL_COUNTER(ret_type, fname, arg)	ret_type fname(arg);
#define BOCHS_BREAKPOINT
#endif /* DEBUG */

NON_NULL_PARAMS noreturn void printPanicMsg(const char *msg, const char *file, const char *func, int line);

#define panic(msg)     printPanicMsg( msg, __FILE__, __func__, __LINE__ )

#endif /* DEBUG_H */
