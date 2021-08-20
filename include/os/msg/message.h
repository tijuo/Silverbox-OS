#ifndef OS_MSG_MESSAGE_H
#define OS_MSG_MESSAGE_H

#include <types.h>

#define RESPONSE_OK		0
#define RESPONSE_FAIL	1

typedef struct
{
  int subject;
  tid_t sender;
  tid_t recipient;
  void *buffer;
  size_t bufferLen;
  size_t bytesTransferred;
  int flags;
} msg_t;

#define EMPTY_MSG { .subject = 0,\
                    .sender = NULL_TID,\
                    .recipient = NULL_TID,\
                    .buffer = NULL,\
                    .bufferLen = 0,\
                    .bytesTransferred = 0,\
                    .flags = 0 }

#endif /* OS_MSG_MESSAGE_H */
