#include <oslib.h>
#include <os/message.h>
#include <os/region.h>
#include <os/services.h>
#include <stdlib.h>
#include "connection.h"

#define NULL_MID -1

#define ALLOC_MBOX(mid, msgQueue, queueLen) \
char _len = ((queueLen == 0) ? 1 : queueLen); \
msgQueue.queueAddr = malloc(_len * sizeof(struct Message)); \
msgQueue.pointer = msgQueue.tail = 0; \
msgQueue.length = _len; \
if( msgQueue.queueAddr == NULL ) \
  mid = NULL_MID; \
else { \
  mid = __alloc_mbox( 0, &msgQueue ); \
  if( mid == NULL_MID ) \
    free( msgQueue.queueAddr ); \
}

#define RELEASE_MBOX(mid, msgQueue) \
free(msgQueue.queueAddr); \
__release_mbox(mid);

#define SET_HEADER(header, subj, rep, data_len) \
header.subject = subj; \
header.reply = rep; \
header.length = data_len;

/* Maybe use __connect() for synchronous and __connectA() for asynchronous */

shmid_t __connect(mid_t server_mbox, struct MemRegion *region, size_t pages)
{
  mid_t mbox;
  struct MessageQueue msgQueue;
  struct Message msg;
  shmid_t shmid;
  
  struct {
    struct MessageHeader header;
    union {
      struct ConnectArgs args;
      struct ShmRegInfo shmreg_info;
    };
  } data;

  if( region == NULL )
    return -1;

  ALLOC_MBOX(mbox, msgQueue, 1);

  SET_HEADER( data.header, CONNECT_REQ, mbox, sizeof data );

  data.args.region = *region;
  data.args.pages = pages;

  _post_message( &__out_queue, server_mbox, &data, sizeof data );
  __send();
  __test_for_msg( mbox, true );
  __get_message( &msgQueue, &msg );
  shmid = *(shmid_t *)(&msg.data + sizeof(struct MessageHeader));
  
  RELEASE_MBOX(mbox, msgQueue);
  
  return shmid;
}

int __disconnect(struct ShmRegInfo *shmreg_info)
{
  return -1;
// TODO: __disconnect() needs to be implemented. The code below is the same for __connect()
/*
  mid_t mbox;
  struct MessageQueue msgQueue;
  struct Message msg;
  
  struct {
    struct MessageHeader header;
    union {
      struct ConnectArgs args;
      struct ShmRegInfo shmreg_info;
    };
  } data;

  if( shmreg_info == NULL )
    return -1;

  ALLOC_MBOX(mbox, msgQueue, 1);

  SET_HEADER( data.header, DISCONNECT_REQ, mbox, sizeof data );

  data.args.length = length;

  _post_message( &__out_queue, other_mbox, &data, sizeof data );
  __send();
  __test_for_msg( &msgQueue, true );
  __get_message( &msgQueue, &msg );
  *shmreg_info = *(struct ShmRegInfo *)msg.data; // doesn't check to see if 
  
  RELEASE_MBOX(mbox, msgQueue);
  
  return 0;
*/
}
