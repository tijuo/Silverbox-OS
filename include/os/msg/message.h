#ifndef OS_MSG_MESSAGE_H
#define OS_MSG_MESSAGE_H

#include <types.h>

#define RESPONSE_OK		    0
#define RESPONSE_FAIL	    1
#define RESPONSE_ERROR      2

#define ANY_SENDER          NULL_TID

#define MSG_NOBLOCK         0x01u
#define MSG_EMPTY           0x00u   // Only subject is sent
#define MSG_STD             0x02u   // MMX registers contain data (64 bytes)
#define MSG_LONG            0x04u   // SSE registers contain data (128 bytes)
#define MSG_HUGE            0x06u   // Both MMX and SSE registers contain data (192 bytes)
#define MSG_KERNEL          0x80u

typedef struct
{
  uint32_t subject;
  uint8_t flags;

  union Target {
    tid_t recipient;
    tid_t sender;
  } target;

  union Payload {
    uint64_t uint64[24];
    int64_t  int64[24];
    uint32_t uint32[48];
    int32_t  int32[48];
    uint16_t uint16[96];
    int16_t  int16[96];
    uint8_t  uint8[192];
    int8_t   int8[192];
  } payload;
} msg_t;

#define EMPTY_MSG { \
  .subject = 0,\
  .flags = 0,\
  .target = { \
    .sender = NULL_TID \
  }, \
  .payload = { \
    .uint64 = { \
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
      0, 0, 0, 0 \
    } \
  } \
}

/// Store the message payload in the SIMD registers

void setMessagePayload(const msg_t *msg);

/// Retrieve the message payload from the SIMD registers
void getMessagePayload(msg_t *msg);

/// Indicate that we're done using the SIMD registers (in particular, MMX)
void finishMessagePayload(void);

/*
#define EMPTY_REQUEST_MSG(subj, recv) { .subject = subj, .sender = NULL_TID, .recipient = recv, .buffer = NULL, .bufferLen = 0, .flags = 0, .bytesTransferred = 0 }
#define REQUEST_MSG(subj, recv, request) { .subject = subj, .sender = NULL_TID, .recipient = recv, .buffer = &request, .bufferLen = sizeof request, .flags = 0, .bytesTransferred = 0 }
#define RESPONSE_MSG(response)  { .subject = 0, .sender = NULL_TID, .recipient = NULL_TID, .buffer = &response, .bufferLen = sizeof response, .flags = 0, .bytesTransferred = 0 }
*/

#endif /* OS_MSG_MESSAGE_H */
