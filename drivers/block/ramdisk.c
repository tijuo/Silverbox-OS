#include <oslib.h>
#include <os/dev_interface.h>
#include <drivers/ramdisk.h>
#include <string.h>
#include <stdlib.h>
#include <os/device.h>
#include <os/services.h>
#include <os/message.h>

#define MSG_TIMEOUT		3000

char buffer1[4096];

extern unsigned char floppy_array[];

#define RAMDISK_SERVER 		RAMDISK_MAJOR
#define NUM_DEVICES		NUM_RAMDISKS
#define SERVER_NAME		"ramdisk"
#define DEVICE_NAME		"rd"

#define BLK_LEN			RAMDISK_BLKSIZE

void handleDevRequests( void );
void handle_dev_read( volatile struct Message *msg );
void handle_dev_write( volatile struct Message *msg );
void handle_dev_ioctl( volatile struct Message *msg );
void handle_dev_error( volatile struct Message *msg );

/*
tid_t video_srv = NULL_TID;

int printMsg( char *msg )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, strlen(msg), 1, msg );
}
*/

void handle_dev_read( volatile struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  size_t num_bytes;
  tid_t tid = msg->sender;
  unsigned char devNum = req->deviceNum;
  size_t offset = req->offset;

  num_bytes = req->count;// > sizeof kbMsgBuffer ? sizeof kbMsgBuffer : req->count;
  num_bytes *= BLK_LEN;

  msg->length = sizeof *req;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;

  if( sendMsg( tid, (void *)msg, MSG_TIMEOUT ) < 0 )
    return;

  if( sendLong( tid, &ramdiskDev[devNum].data[offset*ramdiskDev[devNum].blockSize],
         num_bytes, MSG_TIMEOUT ) < 0 )
  {
    return;
  }
}

void handle_dev_write( volatile struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  tid_t tid = msg->sender;
  unsigned char devNum = req->deviceNum;
  size_t offset = req->offset;

  msg->length = sizeof *req;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;

  if( sendMsg( tid, (void *)msg, MSG_TIMEOUT ) < 0 )
    return;

  if( receiveLong( tid, &ramdiskDev[devNum].data[offset*
     ramdiskDev[devNum].blockSize], ramdiskDev[devNum].blockSize *
     (ramdiskDev[devNum].numBlocks - offset), MSG_TIMEOUT ) )
  {
    return;
  }
}

void handle_dev_ioctl( volatile struct Message *msg )
{
  handle_dev_error(msg);
}

void handle_dev_error( volatile struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  tid_t tid = msg->sender;

  msg->length = 0;
  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_ERROR;

  if( sendMsg( tid, (void *)msg, MSG_TIMEOUT ) < 0 )
    return;
}

void handleDevRequests( void )
{
  volatile struct Message msg;
  volatile struct DeviceMsg *req = (volatile struct DeviceMsg *)msg.data;

  if( receiveMsg(NULL_TID, (void *)&msg, -1) < 0 )
    return;

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
      mapMem((void *)0x6000000, (void *)0x800000, 360, 0); // Only works in bochs!
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
