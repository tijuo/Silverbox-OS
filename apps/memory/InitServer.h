#ifndef INIT_SERVER_H
#define INIT_SERVER_H

extern "C"
{
  #include <oslib.h>
  #include <string.h>
}

#define REGISTER_NAME 3
#define LOOKUP_NAME 4
#define LOOKUP_ID 5

void mapPage( void );

//void *allocEnd;
//unsigned availBytes;
//int sysID;

#endif /* INIT_SERVER_H */

