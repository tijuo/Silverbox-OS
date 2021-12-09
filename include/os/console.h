#ifndef CONSOLE_H

#define CONSOLE_H

#include "../type.h"

#define TAB_WIDTH           4

int printChar( char c );
int printStrN( char *str, size_t len );
int printMsg( char *msg );
char kbConvertRawChar( unsigned char c );
char kbGetRawChar( void );
char kbGetChar( void );

extern tid_t video_srv;
extern tid_t kb_srv;

#endif /* CONSOLE_H */
