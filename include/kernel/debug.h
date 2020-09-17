#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG

#include <kernel/mm.h>
#include <kernel/thread.h>
#define  VIDMEM_START   0xB8000u

#define PRINT_DEBUG(msg)      ({ kprintf("<'%s' %s: %d> %s", \
                              __FILE__, __func__, __LINE__, (msg)); })
#define RET_MSG(ret, msg)     ({ kprintf("<'%s' %s: line %d> Returned: %d. %s\n", \
				              __FILE__, __func__, __LINE__, (ret), (msg)); \
				              return ret; })

#define DECL_CALL_COUNTER(fname)		extern unsigned int fname ## _counter;
#define CALL_COUNTER(fname)			unsigned int fname ## _counter;
#define INC_COUNT()				__func__ _counter++

void init_serial(void);
int getDebugChar(void);
void putDebugChar(int ch);

DECL_CALL_COUNTER(incSchedCount)
void incSchedCount( void );
void incTimerCount( void );
void clearScreen( void );
void kprintf( const char *str, ... );
void printAssertMsg(const char *exp, const char *file, const char *func, int line);
void setBadAssertHlt( bool value );
void setVideoLowMem( bool value );
void dump_regs( const tcb_t *thread, const ExecutionState *state, int intNum, int errorCode );
void dump_state( const ExecutionState *state, int intNum, int errorCode );

/*
char *_toHexString(unsigned int num);
char *__toHexString(unsigned int, int);
char *_toIntString(int num);
char *__toIntString(int, int);
void putChar( char, int, int );
void _putChar( char, int, int, unsigned char );
*/
void printString(const char *, ...);
void initVideo( void );

#define rdtsc( upper, lower ) asm __volatile__( "rdtsc\n" : "=a"( *lower ), "=d"( *upper ) )

void startTimeStamp(void);
void stopTimeStamp(void);
unsigned int getTimeDifference(void);

#define calcTime(function, ret) \
  startTimeStamp(); \
  function; \
  stopTimeStamp();\
  ret = getTimeDifference();

#define assert(exp)  { __typeof__ (exp) _exp=(exp); \
                       if(_exp) ; \
                       else printAssertMsg( #exp, __FILE__, __func__, __LINE__ ); }

#else

#define assert(exp)         ({})
#define incSchedCount()     ({})
#define incTimerCount()     ({})
#define clearScreen()       ({})
#define kprintf( x, ... )   ({})
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
#endif /* DEBUG */

void printPanicMsg(const char *msg, const char *file, const char *func, int line);

#define panic(msg)     printPanicMsg( msg, __FILE__, __func__, __LINE__ )

#endif /* DEBUG_H */
