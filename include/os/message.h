#ifndef OS_MESSAGE_H
#define OS_MESSAGE_H

#include <types.h>
#include <oslib.h>

#define MSG_PROTO_RAW		0
#define MSG_PROTO_GENERIC	1
#define MSG_PROTO_DEVMGR	2
#define MSG_PROTO_DEVICE	3
#define MSG_PROTO_LONG		4
#define MSG_PROTO_VFS		5

struct Message
{
  volatile tid_t sender;
  unsigned short length;
  unsigned short protocol;
//  unsigned char encap_protocol;
  unsigned char data[MAX_MSG_LEN-sizeof(tid_t)-2*sizeof(unsigned short)];
};

#endif /* OS_MESSAGE_H */
