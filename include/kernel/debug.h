#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG

#include <kernel/mm.h>
#define  VIDMEM_START   0xB8000u

#define RET_MSG(ret, msg)	{ kprintf("<'%s' %s: %d> Ret: %d. %s\n", \
				__FILE__, __func__, __LINE__, ret, msg); \
				return ret; }

#define CALL_COUNTER(ret_type, fname, arg)		ret_type fname(arg); int fname ## _counter 
#define INC_COUNT()					__func__ _counter++
//unsigned xVideoPos, yVideoPos;
bool useLowMem;
bool badAssertHlt;

void init_serial(void);
int getDebugChar(void);
void putDebugChar(int ch);

CALL_COUNTER(void, incSchedCount, void);
//void incSchedCount( void );
void incTimerCount( void );
void clearScreen( void );
void kprintf( const char *str, ... );
void printAssertMsg(const char *exp, const char *file, const char *func, int line);
void setBadAssertHlt( bool value );
void setVideoLowMem( bool value );
void dump_regs( const TCB *thread );
void dump_state( const ExecutionState *state );

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

static inline void rdtsc( dword *upper, dword *lower )
{
  asm __volatile__( "rdtsc\n" : "=a"( *lower ), "=d"( *upper ) );
}

dword upper1, lower1, upper2, lower2;

static inline void startTimeStamp(void)
{
  rdtsc(&upper1, &lower1);
}

static inline void stopTimeStamp(void)
{
  rdtsc(&upper2, &lower2);
}

static inline unsigned getTimeDifference(void)
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

#define calcTime(function, ret) \
  startTimeStamp(); \
  function; \
  stopTimeStamp();\
  ret = getTimeDifference();

#define assert(exp)  if (exp) ; \
        else printAssertMsg( #exp, __FILE__, __func__, __LINE__ )

#else

#define assert(exp)
#define incSchedCount()
#define incTimerCount()
#define clearScreen()
#define kprintf( x, ... )
#define printAssertMsg( w, x, y, z )
#define setBadAssertHlt( val )
#define setVideoLowMem( val )
#defin dump_regs( t );
#define dump_state( s );

//#define kprintInt( num )
//#define kprintHex( num )
#define calcTime(func, ret) func
//#define kprint3Nums( str, ... )
//#define putChar(c, i, j)
//#define _putChar(c, i, j, attr)
//#define printString(str, x, y)
//#define _printString(str, x, y, ...)
#define initVideo() 
#define RET_MSG(x,y)	return x;
#define CALL_COUNTER(ret_type, fname, arg)	ret_type fname(arg);
#endif /* DEBUG */

#endif /* DEBUG_H */
