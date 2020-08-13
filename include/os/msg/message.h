#ifndef OS_MSG_MESSAGE_H
#define OS_MSG_MESSAGE_H

#include <types.h>

#define REPLY_OK		0
#define REPLY_FAIL		1

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
    int i32[5];
    short int i16[10];
    char c8[20];
  } data;
} msg_t;

#endif /* OS_MSG_MESSAGE_H */
