#include <oslib.h>
#include <string.h>
#include <os/io.h>
#include <os/services.h>
#include <os/dev_interface.h>
#include <os/message.h>
#include "scancodes.h"
#include <os/keys.h>
#include <os/ostypes/circbuffer.h>
#include <drivers/keyboard.h>
#include <os/msg/init.h>

#define MSG_TIMEOUT		3000

/* Keyboard commands(via interface) */

#define KB_WRITE_OUTPUT		    0x90
#define KB_VERSION_NUM		    0xA1
#define KB_GET_PASSWORD		    0xA4
#define KB_SET_PASSWORD		    0xA5
#define KB_CHECK_PASSWORD	    0xA6
#define MOUSE_DISABLE           0xA7
#define MOUSE_ENABLE		    0xA8
#define MOUSE_TEST              0xAA
#define KB_TEST                 0xAB
#define KB_DISABLE              0xAD
#define KB_ENABLE               0xAE
#define KB_VERSION              0xAF
#define KB_READ_INPUT           0xC0
#define KB_COPY_INPUT_LSN       0xC1
#define KB_COPY_INPUT_MSN       0xC2
#define KB_READ_OUTPUT          0xD1
#define KB_WRITE_BUFFER         0xD2
#define MOUSE_WRITE_BUFFER      0xD3
#define MOUSE_WRITE		        0xD4
#define KB_READ_TEST		    0xE0
#define KB_PULSE_OUTPUT		    0xF0

/* Mouse commands(via interface) */

#define MOUSE_RESET		        0xFF
#define MOUSE_RESEND		    0xFE
#define MOUSE_DEFAULTS	    	0xF6
#define MOUSE_DIS_REPORT	    0xF5
#define MOUSE_EN_REPORT		    0xF4
#define MOUSE_RATE		        0xF3
#define MOUSE_REMOTE	    	0xF0
#define MOUSE_WRAP		        0xEE
#define MOUSE_RESET_WRAP    	0xEC
#define MOUSE_READ_DATA	    	0xEB
#define MOUSE_STREAM	    	0xEA
#define MOUSE_STATUS_REQ    	0xE9
#define MOUSE_DEVICE_ID	    	0xF2
#define MOUSE_RESOLUTION	    0xE8
#define MOUSE_SCALING_2_1	    0xE7
#define MOUSE_SCALING_1_1       0xE6

/* Low level keyboard commands */

#define KB_SCANCODE		        0xF0
#define KB_RESEND		        0xFE
#define KB_RESET		        0xFF
#define KB_ENABLE_SCAN		    0xF4
#define KB_DISABLE_SCAN		    0xF5
#define KB_DEFAULTS		        0xF6
#define KB_LEDS			        0xED
#define KB_TYPERATE		        0xF3
#define KB_READ_ID		        0xF2
#define KB_ECHO			        0xEE
#define KB_MAKE			        0xFD
#define KB_MAKE_BRK		        0xFC
#define KB_TYPEMATIC		    0xFB
#define KB_ALL_TYPE_MK_BRK	    0xFA
#define KB_ALL_MAKE		        0xF9
#define KB_ALL_MAKE_BRK		    0xF8
#define KB_ALL_TYPEMATIC	    0xF7


#define KB_ACK			        0xFA

#define KB_EXT_CODE		        0xE0
#define KB_EXT_BREAK	    	0xF0
//#define KB_
#define KB_IO			        0x60
#define KB_STAT_CMD  	    	0x64

#define KB_ACK      	    	0xFA
#define KB_NACK     	       	0xFE
#define KB_DNACK    		    0xFC

#define KEYBOARD_ID 		    KEYBOARD_MAJOR
#define NUM_DEVICES		        1
#define DEV_MAJOR		        KEYBOARD_ID

#define KB_MAX_BUFSIZE          2048
#define MOUSE_MAX_BUFSIZE	    2048

#define sendKB_Data(data) \
  if(waitForKbInput() == 0) \
    outByte(KB_IO, data);

#define sendKB_Comm(comm) \
  if(waitForKbInput() == 0) \
    outByte(KB_STAT_CMD,comm);

#define enable_kb() \
  sendKB_Comm(KB_ENABLE); \
  getAck();

#define enable_mouse() \
  sendKB_Comm(MOUSE_ENABLE); \
  getAck();

#define send_mouse(comm)        sendKB_Comm( 0xD4 ); \
				sendKB_Data(comm );
//				getAck();

byte kbBufData[KB_MAX_BUFSIZE];
byte mouseBufData[MOUSE_MAX_BUFSIZE];

CircularBuffer kbBuffer = {
    .data = kbBufData,
    .ptr = kbBufData,
    .unreadLen = 0,
    .bufLen = KB_MAX_BUFSIZE
};

CircularBuffer mouseBuffer = {
    .data = mouseBufData,
    .ptr = mouseBufData,
    .unreadLen = 0,
    .bufLen = MOUSE_MAX_BUFSIZE
};

int getAck(void);
int waitForKbInput(void);

enum ScancodeSet { XT_SET, AT_SET };

enum ScancodeSet scancode_set = XT_SET;

void handleDevRequests( void );

void handle_dev_read( struct Message *msg );
void handle_dev_write( struct Message *msg );
void handle_dev_ioctl( struct Message *msg );
void handle_dev_error( struct Message *msg );

char kbMsgBuffer[4096];

/* Assumes buffer and outBytes are properly initialized and not NULL. */

int waitForKbInput(void)
{
  int j = 20;

  while(j && (inByte(KB_STAT_CMD) & 0x02))
  {
    for(int i=0; i < 100; i++ );
    j--;
  }

  if( j == 0 )
    return -1;
  else
    return 0;
}

int waitOutputEmpty(void)
{
  int j = 20;

  while(j && (inByte(KB_STAT_CMD) & 0x01))
  {
    for(int i=0; i < 100; i++ );
    j--;
  }

  if( j == 0 )
    return -1;
  else
    return 0;
}

void setScancode( int scode )
{
  sendKB_Data(KB_SCANCODE);
  sendKB_Data( scode );
}

byte getScancode()
{
  sendKB_Data(KB_SCANCODE);
  sendKB_Data( 0 );

  while( waitOutputEmpty() );
  return inByte( KB_IO );
}

int getAck(void)
{
  switch(inByte(KB_IO))
  {
    case KB_ACK:
      return 0;
    default:
    case KB_NACK:
      return -1;
    case KB_DNACK:
      return -2;
  }
}

void signal_handler(int signal, int arg)
{
  static int ext1=0, ext2=0;

  if( (signal & 0xFF) == SIGINT )
  {
    if(((signal >> 8) & 0xFF) == 0x21)
    {
      byte key = inByte(KB_IO);

      if( scancode_set == XT_SET )
      {
        if( (key & 0xE0) == 0xE0 )
        {
          if( ext1 == 0 || ext1 == 1 )
            ext1++;
        }
        else if( (key & 0xE1) == 0xE1 )
          ; // TODO: need to implement Pause
        else
        {
          if( ext1 == 0 )
          {
            putKeyCode( xt_scancodes[key & 0x7F] |
               ((key & 0x80) ? VK_BREAK : VK_MAKE) );
            ext1 = 0;
          }
          else if( ext1 == 1 )
          {
            switch( key & 0x7F )
            {
              case 0x1C:
                putKeyCode( VK_KP_ENTER | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
		ext1 = 0;
                break;
              case 0x35:
                putKeyCode( VK_KP_SLASH | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
		ext1 = 0;
                break;
              case 0x4D:
                putKeyCode( VK_RIGHT | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
		ext1 = 0;
                break;
              case 0x51:
                putKeyCode( VK_PAGEDN | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
		break;
              case 0x49:
                putKeyCode( VK_PAGEUP | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x50:
                putKeyCode( VK_DOWN | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
		ext1 = 0;
                break;
              case 0x48:
                putKeyCode( VK_UP | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x4F:
                putKeyCode( VK_END | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x47:
                putKeyCode( VK_HOME | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x4B:
                putKeyCode( VK_LEFT | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x53:
                putKeyCode( VK_DELETE | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x52:
                putKeyCode( VK_INSERT | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x1D:
                putKeyCode( VK_RCTRL | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x38:
                putKeyCode( VK_RALT | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
                ext1 = 0;
                break;
              case 0x2A:
               break;
              default:
                ext1 = 0;
                break;
            }
          }
          if( ext1 == 2 )
          {
            ext1 = 0;

            if( (key & 0x7F) == 0x37 )
              putKeyCode( VK_PRNSCR | ((key & 0x80) ? VK_BREAK : VK_MAKE) );
          }
        }
      }
      __end_irq( 0x21 );
    }
/*
    else if( ((signal >> 8) & 0xFFFF) == 0x2C )
      handleMouse();
*/
  }
}

void handle_dev_read( struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  size_t num_chars;
  volatile byte code;
  tid_t tid = msg->sender;

  num_chars = req->count > sizeof kbMsgBuffer ? sizeof kbMsgBuffer : req->count;

  msg->length = sizeof *req;
  req->count = num_chars;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;

  if( sendMsg( tid, msg, MSG_TIMEOUT ) < 0 )
    return;

  for(int i=0; i < num_chars; i++)
  {
    while((code=getKeyCode()) == (byte)-1) // FIXME: potentially unsafe
      __pause();
    kbMsgBuffer[i] = code;
  }

  if( sendLong( tid, kbMsgBuffer, sizeof kbMsgBuffer, MSG_TIMEOUT ) < 0 )
    return;
}

void handle_dev_write( struct Message *msg )
{
  handle_dev_error(msg);
}

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

  if( sendMsg( tid, msg, MSG_TIMEOUT ) < 0 )
    return;
}

void handleDevRequests( void )
{
  volatile struct Message msg;
  volatile struct DeviceMsg *req = (volatile struct DeviceMsg *)msg.data;

  while(1)
  {
    if( receiveMsg(NULL_TID, &msg, -1) < 0 )
    {
      print("fail\n");
      return;
    }

    if( msg.protocol == MSG_PROTO_DEVICE && (req->msg_type & 0x80) == DEVICE_REQUEST )
    {
      switch(req->msg_type)
      {
        case DEVICE_WRITE:
          handle_dev_write((struct Message *)&msg);
          break;
        case DEVICE_READ:
          handle_dev_read((struct Message *)&msg);
          break;
        case DEVICE_IOCTL:
          handle_dev_ioctl((struct Message *)&msg);
          break;
        default:
          handle_dev_error((struct Message *)&msg);
          break;
      }
    }
  }
}

int main( void )
{
//  struct Device dev;
  int status;

//  changeIoPerm( KB_IO, KB_IO, 1 );
//  changeIoPerm( KB_STAT_CMD, KB_STAT_CMD, 1 );

//  __map(0xB8000,0xB8000, 8);

//  mapMem( (void *)0xB8000, (void *)0xB8000, 8, 0 );

//  __register_int( 0x21 );
//  __register_int( 0x2C );

  for( int i=0; i < 5; i++ )
  {
    status = registerName(KEYBOARD_NAME, strlen(KEYBOARD_NAME));

    if( status != 0 )
      sys_sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;

  /*
  dev.major = DEV_MAJOR;
  dev.numDevices = NUM_DEVICES;
  dev.dataBlkLen = 1;
  dev.type = CHAR_DEV;
  dev.cacheType = NO_CACHE;

  for( int i=0; i < 5; i++ )
  {
    status = registerDevice(KEYBOARD_NAME, strlen(KEYBOARD_NAME), &dev);

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;
*/

  //set_signal_handler(&signal_handler);
  //__register_int(0x21);
  registerIrq(0x21);

  while(1)
    handleDevRequests();

  return 1;
}
