#include <oslib.h>
#include <os/dev_interface.h>
#include <string.h>
#include <os/message.h>

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

  while( __send( tid, (struct Message *)&msg, 0 ) == 2 );
  while( __receive( tid, (struct Message *)&msg, 0 ) == 2 );

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
    num_read = _receive( tid, buffer, num_blks * blk_len, 0 );
//      num_read += request->length;

//    memcpy( (void *)((unsigned)buffer + buf_offset), (void *)(request + 1), request->length );
//    buf_offset += request->length;
  /*} while( request->msg_type & DEVICE_MORE );*/

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

  //msg.length = sizeof *request + (sizeof *request + ((num_blks * blk_len) - bytes_written) >
  //               sizeof msg.data ? sizeof msg.data - sizeof *request : (num_blks * blk_len) - bytes_written );

  msg.length = sizeof *request;

/*
  do
  {
    if( minus )
      memcpy( (void *)(request + 1), (void *)((unsigned)buffer + buf_offset), msg.length - minus );
    else
      memcpy( (void *)msg.data, (void *)((unsigned)buffer + buf_offset), msg.length );
*/
    while( __send( tid, (struct Message *)&msg, 0 ) == 2 );
    while( __receive( tid, (struct Message *)&msg, 0 ) == 2 );

    if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
      return -1;
    else
      bytes_written = _send( tid, buffer, blk_len * num_blks, 0 );
/*
    {
      bytes_written += request->count;// * blk_len;
      buf_offset += request->count;// * blk_len;
    }

    msg.length = (((num_blks * blk_len) - bytes_written) >
                 sizeof msg.data ? sizeof msg.data : (num_blks * blk_len) - bytes_written );
    minus = 0;

  } while( request->msg_type & DEVICE_MORE );
*/
  return bytes_written;
}

// XXX: Not complete : needs to do something (we can send data, but how to receive data?)

int deviceIoctl( tid_t tid, unsigned char device, int command, void *args, 
                 size_t args_len )
{
  struct DeviceMsg *request;
  struct Message msg;

  request = (struct DeviceMsg *)msg.data;

  request->msg_type = DEVICE_IOCTL | DEVICE_REQUEST;
  request->deviceNum = device;
  request->command = command;
  request->args_len = args_len;

  msg.length =  sizeof *request; /*+ (sizeof *request + args_len >
               sizeof msg.data ? sizeof msg.data - sizeof *request : args_len);*/
  msg.protocol = MSG_PROTO_DEVICE;

  memcpy( (request + 1), args, msg.length - sizeof *request );

  while( __send( tid, &request, msg.length - sizeof *request ) == 2 );
  while( __receive( tid, &msg, 0 ) == 2 );

  if( (request->msg_type & DEVICE_ERROR) == DEVICE_ERROR )
    return -1;
  else
  {
    if( args_len > 0 )
      _send(tid, args, args_len, 0 );

    if( request->length > 0 )
     ;// ;_receive(tid
  }
  
  return 0;
}
