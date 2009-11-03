#include <oslib.h>
#include <os/dev_interface.h>
#include <drivers/ramdisk.h>
#include <string.h>
#include <stdlib.h>
#include <os/device.h>
#include <os/services.h>
#include <os/message.h>

char buffer1[4096];

extern unsigned char floppy_array[];

#define RAMDISK_SERVER 		RAMDISK_MAJOR
#define NUM_DEVICES		NUM_RAMDISKS
#define SERVER_NAME		"ramdisk"
#define DEVICE_NAME		"ramdisk"

#define BLK_LEN			RAMDISK_BLKSIZE

void handleDevRequests( void );
void handle_dev_read( struct Message *msg );
void handle_dev_write( volatile struct Message *msg );
void handle_dev_ioctl( struct Message *msg );
void handle_dev_error( struct Message *msg );

/*
tid_t video_srv = NULL_TID;

int printMsg( char *msg )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, strlen(msg), 1, msg );
}
*/

void handle_dev_read( struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  size_t num_bytes;
  volatile byte code;
  tid_t tid = msg->sender;
  unsigned char devNum = req->deviceNum;
  size_t offset = req->offset;

  num_bytes = req->count;// > sizeof kbMsgBuffer ? sizeof kbMsgBuffer : req->count;
  num_bytes *= BLK_LEN;

  msg->length = sizeof *req;
//  req->count = num_bytes;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;
  while( __send( tid, msg, 0 ) == 2 );
/*
  for(int i=0; i < num_chars; i++)
  {
    while((code=getKeyCode()) == (byte)-1) // FIXME: potentially unsafe
      __pause();
    kbMsgBuffer[i] = code; // FIXME: ad-hoc; this WILL break on large requests
  }
*/
  _send( tid, &ramdiskDev[devNum].data[offset*ramdiskDev[devNum].blockSize], 
         num_bytes, 0 );
}

void handle_dev_write( volatile struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  size_t count;
  tid_t tid = msg->sender;
  unsigned char devNum = req->deviceNum;
  size_t offset = req->offset;

/*  req->count = (req->count > sizeof msg->data - 
     sizeof *req ? sizeof msg->data - sizeof *req : 
     req->count);*/

  msg->length = sizeof *req;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;
  while( __send( tid, msg, 0 ) == 2 );

  _receive( tid, &ramdiskDev[devNum].data[offset*
     ramdiskDev[devNum].blockSize], ramdiskDev[devNum].blockSize * 
     (ramdiskDev[devNum].numBlocks - offset), 0 );

/*  for(int i=0; i < count; i++)
    printChar(videoMsgBuffer[i]);*/
}

/*
void handle_dev_read( struct Message *msg )
{
  volatile struct Message reply_msg;
  volatile struct DeviceMsg *reply = (volatile struct DeviceMsg *)reply_msg.data;
  volatile struct DeviceMsg *request = (volatile struct DeviceMsg *)msg->data;
  size_t num_blocks = request->count, offset = request->offset;
  unsigned char dev_num = request->deviceNum;
  size_t blk_offset=0, msg_offset=0,bytes_read=0;
  tid_t sender = msg->sender;

  if( dev_num > NUM_RAMDISKS )
    handle_dev_error(msg);

  if( offset + num_blocks >= ramdiskDev[dev_num].numBlocks )
    handle_dev_error(msg);

  reply_msg.protocol = MSG_PROTO_DEVICE;
  *reply = *request;
  reply->msg_type = DEVICE_RESPONSE | DEVICE_SUCCESS;
  reply_msg.length = sizeof *reply;

  msg_offset = sizeof *reply;

  for( int i=0; i < num_blocks; )
  {
    if( msg_offset == sizeof reply_msg.data && blk_offset == 0 )
    {
      reply->count = bytes_read;

      if( i != num_blocks )
        reply->msg_type |= DEVICE_MORE;
      else
        reply->msg_type &= ~DEVICE_MORE;

      while( __send(sender, &reply_msg, 0) == 2 );
      bytes_read = 0;
      msg_offset = sizeof *reply;
      reply_msg.length = sizeof *reply;
    }
    else if( blk_offset == 0 && msg_offset + ramdiskDev[dev_num].blockSize - blk_offset <= sizeof reply_msg.data ) // if we can put an entire block at once
    {
      memcpy( (void *)((unsigned)reply_msg.data + msg_offset),
         &ramdiskDev[dev_num].data[(offset+i)*
         ramdiskDev[dev_num].blockSize],
         ramdiskDev[dev_num].blockSize );

      msg_offset += ramdiskDev[dev_num].blockSize;
      bytes_read += ramdiskDev[dev_num].blockSize;
      reply_msg.length += ramdiskDev[dev_num].blockSize;
      i++;

      if( i == num_blocks )
      {
        reply->count = bytes_read;
        reply->msg_type &= ~DEVICE_MORE;

        while( __send(sender, &reply_msg, 0) == 2 );
      }      
    }
    else if( msg_offset + ramdiskDev[dev_num].blockSize - blk_offset > sizeof reply_msg.data ) // if the block is fragmented(split between messages)
    {
      memcpy( (void *)((unsigned)reply_msg.data + msg_offset),
         &ramdiskDev[dev_num].data[(offset+i)*
         ramdiskDev[dev_num].blockSize+blk_offset],
         sizeof msg->data - msg_offset );

      reply_msg.length += (sizeof msg->data - msg_offset);
      bytes_read += (sizeof msg->data - msg_offset);
      blk_offset += (sizeof msg->data - msg_offset);
//      msg_offset = 0;

      reply->msg_type |= DEVICE_MORE;
      reply->count = bytes_read;
      while( __send(tid, &reply_msg, 0) == 2 );

      msg_offset = sizeof *reply;
      reply_msg.length = sizeof *reply;
      bytes_read = 0;
    }
    else // if the end of a fragment
    {
      memcpy( (void *)((unsigned)reply_msg.data + msg_offset),
         &ramdiskDev[dev_num].data[(offset+i)*
         ramdiskDev[dev_num].blockSize+blk_offset],
         ramdiskDev[dev_num].blockSize - blk_offset );

      reply_msg.length += ramdiskDev[dev_num].blockSize - blk_offset;
      bytes_read += ramdiskDev[dev_num].blockSize - blk_offset;
      msg_offset += ramdiskDev[dev_num].blockSize - blk_offset;
      blk_offset = 0;

      i++;

      if( i == num_blocks )
      {
        reply->count = bytes_read;
        reply->msg_type &= ~DEVICE_MORE;

        while( __send(sender, &reply_msg, 0) == 2 );
      }
    }
  }
}


void handle_dev_write( volatile struct Message *msg )
{
  struct Message reply_msg;
  struct DeviceMsg *reply = (struct DeviceMsg *)reply_msg.data;
  struct DeviceMsg *request = (struct DeviceMsg *)msg->data;
  size_t num_blocks = request->count, offset = request->offset;
  unsigned char dev_num = request->deviceNum;
  size_t blk_offset=0, msg_offset=0;
  unsigned char *block;
  size_t bytes_written=0;
  tid_t sender;

  sender = msg->sender;

  if( dev_num > NUM_RAMDISKS )
    handle_dev_error(msg);

  if( offset + num_blocks >= ramdiskDev[dev_num].numBlocks )
    handle_dev_error(msg);

  block = malloc(ramdiskDev[dev_num].blockSize);

  if(block == NULL)
    handle_dev_error(msg);

  *reply = *request;

  reply->msg_type = DEVICE_RESPONSE | DEVICE_SUCCESS;
  reply_msg.protocol = MSG_PROTO_DEVICE;
  reply_msg.length = sizeof *reply;

  msg_offset = sizeof *reply;

  for( int i=0; i < num_blocks; )
  {
    if( blk_offset == 0 && msg_offset == sizeof reply_msg.data )
    {
      reply->count = bytes_written;

      if( i != num_blocks )
        reply->msg_type |= DEVICE_MORE;
      else
        reply->msg_type &= ~DEVICE_MORE;

      while( __send(sender, &reply_msg, 0) == 2 );
      while( __receive(sender, msg, 0) == 2 );

      bytes_written = 0;
      msg_offset = 0;
      reply_msg.length = 0;
    }
    else if( blk_offset == 0 && msg_offset + ramdiskDev[dev_num].blockSize <= sizeof reply_msg.data )
    { // if we can read a whole block...
      memcpy( &ramdiskDev[dev_num].data[(offset + i)*
       ramdiskDev[dev_num].blockSize],
       (void *)((unsigned)msg->data + msg_offset),
       ramdiskDev[dev_num].blockSize );

      msg_offset += ramdiskDev[dev_num].blockSize;
      bytes_written += ramdiskDev[dev_num].blockSize;
      reply_msg.length += ramdiskDev[dev_num].blockSize;

      i++;

      if( i == num_blocks )
      {
        reply->count = bytes_written;
        reply->msg_type &= ~DEVICE_MORE;

        while( __send(sender, &reply_msg, 0) == 2 );
        while( __receive(sender, msg, 0) == 2 );
      }
    }
    else if( msg_offset + ramdiskDev[dev_num].blockSize - blk_offset > sizeof reply_msg.data ) // if the block is fragmented(split between messages)
    {
      memcpy( (void *)((unsigned)block + blk_offset),
              (void *)((unsigned)msg->data + msg_offset),
              sizeof msg->data - msg_offset );

      blk_offset += (sizeof msg->data - msg_offset);
      bytes_written += (sizeof msg->data - msg_offset);
      reply_msg.length += (sizeof msg->data - msg_offset);
      msg_offset = 0;

      reply->count = bytes_written;
      reply->msg_type |= DEVICE_MORE;

      while( __send(sender, &reply_msg, 0) == 2 );
      while( __receive(sender, msg, 0) == 2 );

      bytes_written = 0;
      reply_msg.length = 0;
    }
    else // if the last piece of a fragment...
    {
      memcpy( &ramdiskDev[dev_num].data[(offset + i)*
         ramdiskDev[dev_num].blockSize], block,
         blk_offset );

      memcpy( &ramdiskDev[dev_num].data[(offset + i)*
         ramdiskDev[dev_num].blockSize+blk_offset],
         (void *)((unsigned)msg->data + msg_offset),
         ramdiskDev[dev_num].blockSize - blk_offset );

      bytes_written += ramdiskDev[dev_num].blockSize - blk_offset;
      msg_offset += (ramdiskDev[dev_num].blockSize - blk_offset);
      reply_msg.length +=  (ramdiskDev[dev_num].blockSize - blk_offset);
      blk_offset = 0;

      i++;

      if( i == num_blocks )
      {
        reply->count = bytes_written;
        reply->msg_type &= ~DEVICE_MORE;

        while( __send(sender, &reply_msg, 0) == 2 );
//        while( __receive(sender, msg, 0) == 2 );
      }
    }
  }
  free(block);
}
*/
void handle_dev_ioctl( struct Message *msg )
{
  handle_dev_error(msg);
}

void handle_dev_error( struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  tid_t tid = msg->sender;

  msg->length = 0;
  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_ERROR;
  while( __send( tid, msg, 0 ) == 2 );
}

void handleDevRequests( void )
{
  volatile struct Message msg;
  volatile struct DeviceMsg *req = (volatile struct DeviceMsg *)msg.data;

  while( __receive(NULL_TID, &msg, 0) == 2 );

  if( msg.protocol == MSG_PROTO_DEVICE && (req->msg_type & 0x80) == DEVICE_REQUEST )
  {
    switch(req->msg_type)
    {
      case DEVICE_WRITE:
        handle_dev_write(&msg);
        break;
      case DEVICE_READ:
        handle_dev_read(&msg);
        break;
      case DEVICE_IOCTL:
        handle_dev_ioctl(&msg);
        break;
      default:
        handle_dev_error(&msg);
        break;
    }
  }
}

int main(void)
{
  struct Device devInfo;
  int status;

  //printMsg("Starting Ramdisk\n");

  for( int i=0; i < NUM_RAMDISKS; i++ )
  {
    if( i != 0 )
    {
      ramdiskDev[i].data = malloc( RAMDISK_SIZE );
  
      if( ramdiskDev[i].data == NULL )
        return 1;
    }
    else
    {
      mapMem(0x6000000, 0x800000, 360, 0); // Only works in bochs!
      ramdiskDev[0].data = (unsigned char *)0x800000;
    }

    ramdiskDev[i].blockSize = RAMDISK_BLKSIZE;
    ramdiskDev[i].numBlocks = RAMDISK_SIZE / RAMDISK_BLKSIZE;
    ramdiskDev[i].devNum = i;
  }

  for( int i=0; i < 5; i++ )
  {
    status = registerName(SERVER_NAME, strlen(SERVER_NAME));      

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;

  devInfo.cacheType = NO_CACHE;
  devInfo.type = BLOCK_DEV;
  devInfo.dataBlkLen = RAMDISK_BLKSIZE;
  devInfo.numDevices = NUM_RAMDISKS;
  devInfo.major = RAMDISK_MAJOR;

//  printMsg("Registering ramdisk...\n");

  for( int i=0; i < 5; i++ )
  {
    status = registerDevice(DEVICE_NAME, strlen(DEVICE_NAME), &devInfo);

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;

  while(1)
    handleDevRequests();

  return 1;
}
