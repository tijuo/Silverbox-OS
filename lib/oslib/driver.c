#include <oslib.h>
#include <os/dev_interface.h>
#include <string.h>
#include <os/message.h>

#define MSG_TIMEOUT	3000

/*
DRV_WRITE

1. A requests to form a connection with B.
1a. A writes data to its shared region and sends a DRV_WRITE message to B.
2. B does the "write device" operation, and sends back a WRITE_RESULT message. (Goto 1a if necessary)
3. A requests to disconnect from B

DRV_READ

1. A requests to form a connection with B.
1a A sends a DRV_READ message to B.
2. B does the 'read device' operation, and sends back a READ_RESULT message to B. (Go to 1a if necessary)
3. A requests to disconnect from B
*/

int _deviceRead( tid_t tid, unsigned char device, unsigned offset, size_t num_blks, 
                size_t blk_len, void *buffer )
{
  volatile struct DeviceMsg *request;
  volatile struct Message msg;
  int /*buf_offset = 0,*/ num_read=0;

  request = (volatile struct DeviceMsg *)msg.data;

  request->deviceNum = device;
  request->msg_type = DEVICE_READ | DEVICE_REQUEST;
  request->offset = offset;
  request->count = num_blks;
  msg.protocol = MSG_PROTO_DEVICE;

  msg.length = sizeof *request;

  if( sendMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( receiveMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
    num_read = receiveLong( tid, buffer, num_blks * blk_len, -1 );

  return num_read;
}

// XXX: This doesn't protect against buffer overflows...

int deviceRead( tid_t tid, unsigned char device, unsigned offset, size_t num_blks, 
		size_t blk_len, void *buffer )
{
  volatile struct DeviceMsg *request;
  volatile struct Message msg;
  int /*buf_offset = 0,*/ num_read=0;

  request = (volatile struct DeviceMsg *)msg.data;

  request->deviceNum = device;
  request->msg_type = DEVICE_READ | DEVICE_REQUEST;
  request->offset = offset;
  request->count = num_blks;
  msg.protocol = MSG_PROTO_DEVICE;

  msg.length = sizeof *request;

  if( sendMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( receiveMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
    num_read = receiveLong( tid, buffer, num_blks * blk_len, MSG_TIMEOUT );

  return num_read;
}

// XXX: not complete: Still needs to be able to send multiple messages(for large transfers)

int deviceWrite( tid_t tid, unsigned char device, unsigned offset, size_t num_blks, 
                 size_t blk_len, void *buffer )
{
  volatile struct DeviceMsg *request;
  volatile struct Message msg;
  int bytes_written=0;

  request = (struct DeviceMsg *)msg.data;

  request->deviceNum = device;
  request->msg_type = DEVICE_WRITE | DEVICE_REQUEST;
  request->offset = offset;
  request->count = num_blks;
  msg.protocol = MSG_PROTO_DEVICE;

  msg.length = sizeof *request;

  if( sendMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( receiveMsg(tid, (struct Message *)&msg, MSG_TIMEOUT) < 0 )
    return -1;

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
    bytes_written = sendLong( tid, buffer, blk_len * num_blks, MSG_TIMEOUT );

  return bytes_written;
}

// XXX: Not complete : needs to do something (we can send data, but how to receive data?)

int deviceIoctl( tid_t tid, unsigned char device, int command, void *args,
                 size_t args_len )
{
  return -1;
/*
  struct DeviceMsg *request;
  struct Message msg;

  request = (struct DeviceMsg *)msg.data;

  request->msg_type = DEVICE_IOCTL | DEVICE_REQUEST;
  request->deviceNum = device;
  request->command = command;
  request->args_len = args_len;

  msg.length =  sizeof *request;
  msg.protocol = MSG_PROTO_DEVICE;

  memcpy( (request + 1), args, msg.length - sizeof *request );

  while( __send( tid, &request, msg.length - sizeof *request ) == 2 );
  while( __receive( tid, &msg, 0 ) == 2 );

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
  {
    if( args_len > 0 )
      sendLong(tid, args, args_len, MSG_TIMEOUT );

    if( request->length > 0 )
     ;// ;_receive(tid
  }

  return 0;
*/
}
