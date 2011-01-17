#ifndef OS_MESSAGE_H
#define OS_MESSAGE_H

#include <types.h>
#include <oslib.h>

#define MSG_PROTO_RAW		0
#define MSG_PROTO_GENERIC	1
#define MSG_PROTO_DEVMGR	2 // To be removed
#define MSG_PROTO_DEVICE	3 // To be removed
#define MSG_PROTO_LONG		4
#define MSG_PROTO_VFS		5 // To be removed
#define MSG_PROTO_RPC		6

#define GEN_STAT_OK		0
#define GEN_STAT_ERROR		1	// Bad input arguments or request
#define GEN_STAT_FAIL		2	// Operation/Request did not complete successfully
#define GEN_STAT_SEQ		3	// Wrong sequence number sent
#define GEN_STAT_STOP		4	// Cancel the operation. Do not send any more messages

struct Message
{
  volatile tid_t sender;	// Set by the kernel
  unsigned short length;
  unsigned char protocol;
  unsigned char data[MAX_MSG_LEN-sizeof(tid_t)-sizeof(unsigned short)-sizeof(unsigned char)];
};

struct GenericMsgHeader
{
  int type;	// The request/reply in numeric form. It's meaningful only between the sender/receiver
  int seq;
  char status;

  byte data[];
} __PACKED__;

struct LongMsgHeader
{
  size_t length;
  char reply : 1;
  char fail : 1;
  char _resd : 6;
} __PACKED__;

int sendMsg( tid_t sender, struct Message *msg, int timeout );
int receiveMsg( tid_t recipient, struct Message *msg, int timeout );
int receiveLong( tid_t tid, void *buffer, size_t maxLen, int timeout );
int sendLong( tid_t tid, void *data, size_t len, int timeout );

#endif /* OS_MESSAGE_H */
