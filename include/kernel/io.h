#ifndef IO_H
#define IO_H

#include <types.h>

extern void ioWait(void);
extern byte inByte(word port);
extern word inWord(word port);
extern dword inDword(word port);
extern void outByte(word port, byte data);
extern void outWord(word port, word data);
extern void outDword(word port, dword data);

#endif /* IO_H */
