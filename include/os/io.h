#ifndef OS_IO_H
#define OS_IO_H

#include <types.h>

static inline void ioWait( void );
static inline byte inByte(register word port);
static inline word inWord(register word port);
static inline dword inDword(register word port);
static inline void outByte(register word port, register byte data);
static inline void outWord(register word port, register word data);
static inline void outDword(register word port, register dword data);

static inline void ioWait( void )
{
  __asm__ __volatile__("jmp 1f;1:jmp 1f;1:\n");
}

static inline byte inByte(register word port)
{
  register byte result;
  __asm__ __volatile__("in %%dx, %%al" : "=a" (result) : "d" (port));
  return result;
}

static inline word inWord(register word port)
{
  register word result;
  __asm__ __volatile__("in %%dx, %%ax" : "=a" (result) : "d" (port));
  return result;
}

static inline dword inDword(register word port)
{
  register dword result;
  __asm__ __volatile__("in %%dx, %%eax" : "=a" (result) : "d" (port));
  return result;
}

static inline void outByte(register word port, register byte data)
{
  __asm__ __volatile__("out %%al, %%dx" :: "a" (data),  "d" (port));
}

static inline void outWord(register word port, register word data)
{
  __asm__ __volatile__("out %%ax, %%dx" :: "a" (data),  "d" (port));
}

static inline void outDword(register word port, register dword data)
{
  __asm__ __volatile__("out %%eax, %%dx" :: "a" (data),  "d" (port));
}

#endif /* OS_IO_H */
