#ifndef OS_MSG_MESSAGE_H
#define OS_MSG_MESSAGE_H

#include <types.h>

#define RESPONSE_OK         0
#define RESPONSE_FAIL	    1
#define RESPONSE_ERROR      2

#define ANY_SENDER          NULL_TID
#define ANY_RECIPIENT       NULL_TID

#define MSG_NOBLOCK         0x01u
#define MSG_STD             0x00u
#define MSG_EMPTY           0x02u   // Only subject is sent
#define MSG_KERNEL          0x80u

typedef struct {
    uint32_t subject;

    union {
        tid_t sender;
        tid_t recipient;
    } target;

    uint16_t flags;
    void *buffer;
    size_t buffer_length;
} Message;

typedef Message msg_t;

#define BLANK_MSG { \
  .subject = 0,\
  .target = {\
    .sender = NULL_TID\
  },\
  .flags = 0,\
  .buffer = NULL,\
  .buffer_length = 0\
}

#define EMPTY_REQUEST_MSG(subj, recv) { .subject = subj, .target = { .recipient = recv }, .buffer = NULL, .buffer_length = 0, .flags = MSG_EMPTY }
#define NEW_MSG(subj, recv, request) { .subject = subj, .target = { .recipient = recv }, .buffer = &(request), .buffer_length = sizeof (request), .flags = 0 }
#define NEW_MSG_BUF(subj, recv, request, len) { .subject = subj, .target = { .recipient = recv }, .buffer = request, .buffer_length = len, .flags = 0 }

#endif /* OS_MSG_MESSAGE_H */
