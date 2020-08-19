#ifndef OS_MSG_MESSAGE_H
#define OS_MSG_MESSAGE_H

#include <types.h>

#define RESPONSE_OK		0
#define RESPONSE_FAIL		1

typedef struct
{
  unsigned char subject;
  union
  {
    tid_t sender;
    tid_t recipient;
  };
  union Payload
  {
    unsigned int u32[5];
    unsigned short int u16[10];
    unsigned char u8[20];
    int i32[5];
    short int i16[10];
    char i8[20];
  } data;
} msg_t;

#endif /* OS_MSG_MESSAGE_H */
