#include <oslib.h>
#include <os/message.h>
#include <string.h>
#include <os/io.h>

extern void print(char *);

int _send( tid_t tid, void *data, size_t len, int timeout )
{
  volatile struct LongMsg *long_msg;
  volatile struct Message msg;
  int num_sent = 0;
  int msg_ret_status;

  if( data == NULL || tid == NULL_TID )
    return -1;

  long_msg = (volatile struct LongMsg *)msg.data;

  long_msg->length = len;
  long_msg->fail = 0;
  long_msg->reply = 0;

  msg.protocol = MSG_PROTO_LONG;
  msg.length = len > sizeof msg.data - sizeof *long_msg ? 
               sizeof msg.data : 
               len + sizeof *long_msg;

  num_sent = msg.length - sizeof *long_msg;

  memcpy( (void *)(long_msg + 1), data, num_sent );

  while((msg_ret_status=__send( tid, (struct Message *)&msg, timeout )) == 2);

  if( msg_ret_status < 0 )
    return -1;

  while((msg_ret_status=__receive( tid, (struct Message *)&msg, timeout )) == 2);

  if( msg_ret_status < 0 || long_msg->fail )
    return -1;

  len = long_msg->length < len ? long_msg->length : len;

  if( (int)len <= num_sent )
    return (int)len;

  while( num_sent < (int)len )
  {
    msg.protocol = MSG_PROTO_LONG;

    if( num_sent + sizeof msg.data >= len ) // This is the last message to send
      msg.length = len - num_sent; 
    else
      msg.length = sizeof msg.data;

    memcpy( (void *)msg.data, (void *)((unsigned)data + num_sent), msg.length );
    num_sent += msg.length;

    while((msg_ret_status=__send( tid, (struct Message *)&msg, timeout )) == 2);

    if( msg_ret_status < 0 )
      return (int)num_sent - (int)sizeof msg.data;
  }

  return num_sent;
}

int _receive( tid_t tid, void *buffer, size_t maxLen, int timeout )
{
  volatile struct LongMsg *long_msg;
  volatile struct Message msg;
  size_t num_to_rcv;
  int num_rcvd=0;
  int msg_ret_status;

  if( buffer == NULL )
    return -1;

  long_msg = (volatile struct LongMsg *)msg.data;

  while((msg_ret_status=__receive( tid, (struct Message *)&msg, timeout )) == 2);

  if( msg.protocol != MSG_PROTO_LONG || msg_ret_status < 0 )
  {
    print("Bad protocol!\n");
    return msg_ret_status;
  }

  memcpy( buffer, (void *)(long_msg + 1), 
          msg.length - sizeof *long_msg > maxLen ? maxLen : 
          msg.length - sizeof *long_msg );
  num_rcvd = msg.length - sizeof *long_msg > maxLen ? maxLen : 
               msg.length - sizeof *long_msg;

  num_to_rcv = long_msg->length;

  long_msg->fail = 0;
  long_msg->length = maxLen;
  long_msg->reply = 1;

  msg.length = sizeof *long_msg;

  while((msg_ret_status=__send( tid, (struct Message *)&msg, timeout )) == 2);

  if( msg_ret_status < 0 )
    return -1;

  maxLen = num_to_rcv < maxLen ? num_to_rcv : maxLen;

  if( num_rcvd == (int)maxLen )
    return num_rcvd;

  while( num_rcvd < (int)maxLen )
  {
    while((msg_ret_status=__receive( tid, (struct Message *)&msg, 
           timeout )) == 2);

    if( msg_ret_status < 0 )
      return num_rcvd - sizeof msg.data;

    if( num_rcvd + sizeof msg.data >= maxLen ) // This is the last message to receive
      msg.length = maxLen - num_rcvd;
    else
      msg.length = sizeof msg.data;

    memcpy( (void *)((unsigned)buffer + num_rcvd), (void *)msg.data, 
            msg.length );
    num_rcvd += msg.length;
  }

  return num_rcvd;
}
