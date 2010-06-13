#include <oslib.h>
#include <os/io.h>
#include <os/message.h>
#include <drivers/video.h>
#include <stdlib.h>
#include <stdarg.h>
#include <os/dev_interface.h>
#include <os/services.h>
#include <string.h>
#include <os/signal.h>

/* Video operations:
   Low-level:
   - putAttr( int attr, int pos )
   - putCh( char c, int pos )
   - putCh2( int c, int pos ) // puts character with attribute
   - setCursorPos( int pos )
   - setScreenOffset( int offset )
   - showCursor( bool val )

   Mid-level:
   - putChar( char c, int x, int y );
   - setCursor( int x, int y );
   - setCharPos( int x, int y );
   - scroll( int lines );
   - setAttr( int attrib )
   - setMode( int mode );
   - reset()
   - clear()
*/

#define SERVER_NAME 		"video"
#define DEVICE_NAME		"video"
#define DEV_MAJOR		5
#define NUM_DEVICES		1

#define HTAB_WIDTH 		8

#define VID_PUTCH		0
#define VID_GETCH		1
#define VID_SETCURS		2
#define VID_GET_CURS		3
#define VID_SHOW_CURS		4
#define VID_SCROLL		5
#define VID_SET_ATTR		6
#define VID_SET_MODE		7
#define VID_RESET		8
#define VID_CLEAR		9
#define VID_ENAB_CURS		10

#define doCarrReturn() charXPos = 0;

#define doNewline() \
  charXPos = 0; \
  charYPos++; \
  if( charYPos >= 25 ) scrollToLine( charYPos - 24 );

/* Go to nearest multiple of 8. */

#define doHTab() \
  charXPos += (HTAB_WIDTH - charXPos % HTAB_WIDTH); \
  charYPos = ((charXPos >= maxWidth) ? (charYPos + 1) : charYPos); \
  charXPos %= maxWidth;

#define doFormFeed() \
  charXPos = charYPos = 0; \

#define doVTab() \
  charYPos = charYPos + 1;

#define doBacksp() \
  charYPos -= ((charXPos == 0 && charYPos > 0) ? 1 : 0); \
  charXPos = ((charXPos == 0) ? maxWidth - 1 : charXPos - 1);

#define setAttr( attr ) attrib = attr;

char videoMsgBuffer[4096];

static int charXPos, charYPos;
static int charXPos, charYPos;
static int maxWidth, maxHeight;
static int maxLines;
static bool cursorOn, cursorVisible;
static int attrib;

void printChar( char c );
int putChar( char c, int xPos, int yPos );
int setCursor( int xPos, int yPos );
int setColor( int color, int xPos, int yPos );
void clearScreen( int attrib );
void initVideo( void );
int handleIoctl( int command, int numArgs, void *args );
inline int checkPos( int x, int y );
inline int setCharPos( int x, int y );

void handleDevRequests( void );
void handle_dev_read( struct Message *msg );
void handle_dev_write( struct Message *msg );
void handle_dev_ioctl( struct Message *msg );
void handle_dev_error( struct Message *msg );

int addr_offset=0;

void scrollToLine( unsigned line );
void scroll( int lines );


int setScreenOffset( int offset );

int putAttr( char attr, int pos )
{
  char *vidmem = (char *)COLOR_TXT_ADDR;

  if( pos < 0 || pos >= 0x8000 / 2 )
    return -1;

  vidmem[pos*2+1] = attr;
  return 0;
}

int putCh( char c, int pos )
{
  char *vidmem = (char *)COLOR_TXT_ADDR;

  if( pos < 0 || pos >= 0x8000 / 2 )
    return -1;

  vidmem[pos*2] = c;
  return 0;
}

int putCh2( short int c, int pos )
{
  short int *vidmem = (short int *)COLOR_TXT_ADDR;

  if( pos < 0 || pos >= 0x8000 / 2 )
    return -1;

  vidmem[pos] = c;
  return 0;
}

int setCursorPos( int pos )
{
  if( pos < 0 || pos >= 0x8000 / 2 )
    return -1;

  outByte( CRTC_INDEX, CURSOR_LOC_LOW );
  outByte( CRTC_DATA, pos & 0xFF );
  outByte( CRTC_INDEX, CURSOR_LOC_HIGH );
  outByte( CRTC_DATA, pos >> 8 );
  return 0;
}

int setScreenOffset( int offset )
{
  if( offset < 0 || offset >= 0x8000 )
    return -1;

  addr_offset &= 0xFFFF;

  outByte( CRTC_INDEX, START_ADDRESS_LOW );
  outByte( CRTC_DATA, addr_offset & 0xFF );
  outByte( CRTC_INDEX, START_ADDRESS_HIGH );
  outByte( CRTC_DATA, addr_offset >> 8 ); 

  return 0;
}

int showCursor( bool val )
{
  byte data;

  outByte( CRTC_INDEX, CURSOR_START );
  data = inByte( CRTC_DATA );

  if( val )
    data |= 0x10;
  else
    data &= ~0x10;

  outByte( CRTC_INDEX, CURSOR_START );
  outByte( CRTC_DATA, data );

  return 0;
}

/* ==========================================================================
   ========================================================================== */

void scroll( int lines )
{
  if( addr_offset + lines * 80 < 0 )
    addr_offset = 0;
  else if( addr_offset + lines * 80 >= 0x8000 )
    addr_offset = 0x8000 - 80;
  else
    addr_offset += lines * 80;

  setScreenOffset( addr_offset );
}

void scrollToLine( unsigned line )
{
  addr_offset = line * 80;

  if( addr_offset >= 0x8000 )
    addr_offset = 0x8000 - 80;

  setScreenOffset( addr_offset );
}

int checkPos( int x, int y )
{
  return ( ( x < 0 || y < 0 || x >= maxWidth || y >= 0x8000 / 2 / 80 /*|| y >= maxHeight*/ ) ? -1 : 0 );
}

int setCharPos( int x, int y )
{
  if( checkPos( x, y ) != 0 )
    return -1;
  else
  {
    charXPos = x;
    charYPos = y;
  }
  return 0;
}

void printChar( char c )
{
  if( putChar( c, charXPos, charYPos ) == 0 )
  {
    if( ++charXPos == maxWidth )
    {
      doNewline();
    }
  }

  if( cursorOn )
    setCursor( charXPos, charYPos );
}

int putChar( register char c, int xPos, int yPos )
{
  char *vidmem = (char *)COLOR_TXT_ADDR;

  if( checkPos( xPos, yPos ) == -1 )
    return -1;

  switch( c )
  {
    case '\n':
      doNewline();
      break;
    case '\t':
      doHTab();
      break;
    case '\b':
      doBacksp();
      break;
    case '\f':
      doFormFeed();
      break;
    case '\r':
      doCarrReturn();
      break;
    case '\v':
      doVTab();
      break;
    case '\0':
    case '\a':
      break;
    default:
      vidmem[(xPos + yPos * maxWidth) << 1] = c;
      vidmem[((xPos + yPos * maxWidth) << 1) + 1] = attrib;
      return 0;
  }

  return 1;
}

int setCursor( int xPos, int yPos )
{
    int location;

    if( checkPos( xPos, yPos ) == -1 )
      return -1;

    location = xPos + yPos * maxWidth;

    setCursorPos(location);

    charXPos = xPos;
    charYPos = yPos;

    return 0;
}

int setColor( int color, int xPos, int yPos )
{
  char *vidmem = (char *)COLOR_TXT_ADDR;

  if( checkPos( xPos, yPos ) == -1 )
    return -1;

  vidmem[(xPos + yPos * maxWidth) * 2 + 1] = color;

  return 0;
}

void clearScreen( int attrib )
{
  register int i;

  for( i=0; i < maxHeight; i++ )
  {
    for( int j=0; j < maxWidth; j++ )
    {
      putChar( ' ', j, i );
      setColor( attrib, j, i );
    }
  }
}

void initVideo( void )
{
  maxHeight = 25;
  maxWidth = 80;
  maxLines = maxHeight * 8;
  cursorVisible = true;
  cursorOn = true;
  setAttr( ATTRIB( GRAY, BLACK, NO_BLINK ) );

  //__map( (void *)COLOR_TXT_ADDR, (void *)COLOR_TXT_ADDR, 8 );
//  mapMem( (void *)COLOR_TXT_ADDR, (void *)COLOR_TXT_ADDR, 8, 0 );
//  __chioperm( 1, CRTC_INDEX, 2 ); 	// Allocate port range

  setCursor( 0, 1 );
  setCharPos( 0, 1 );

//  clearScreen( attrib );
}
/*
int handleIoctl( int command, int numArgs, void *args )
{
  return -1;
}

int handleWrite( char *buffer, struct MessageHeader *header )
{
  int i;

  for( i=0; i < header->length; i++ )
    printChar( buffer[i] );

  return i;
}
*/

/*
void handle_dev_read( struct Message *msg )
{

}

void handle_dev_write( struct Message *msg )
{

}

void handle_dev_ioctl( struct Message *msg )
{

}

void handle_dev_error( struct Message *msg )
{

}

void handleDevRequests( void )
{
  struct Message msg;
  struct DeviceMsg *req = (struct DeviceMsg *)msg.data;

  while(1)
  {
    while( __receive(NULL_TID, &msg, 0) == 2 );

    if( req->msg_type )
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
    else
      handle_dev_error(&msg);
  }
}
*/

void handle_dev_read( struct Message *msg )
{
  handle_dev_error(msg);
}

void handle_dev_write( struct Message *msg )
{
  struct DeviceMsg *req = (struct DeviceMsg *)msg->data;
  size_t count;
  tid_t tid = msg->sender;

  msg->length = sizeof *req;

  req->msg_type = (req->msg_type & 0xF) | DEVICE_RESPONSE | DEVICE_SUCCESS;
  while( __send( tid, msg, 0 ) == 2 );

  count = _receive( tid, videoMsgBuffer, sizeof videoMsgBuffer, 0 );

  for(int i=0; i < count; i++)
    printChar(videoMsgBuffer[i]);
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
  while( __send( tid, msg, 0 ) == 2 );
}

void handleDevRequests( void )
{
  struct Message msg;
  struct DeviceMsg *req = (struct DeviceMsg *)msg.data;

  while(1)
  {
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
          print("Received bad request\n");
          handle_dev_error(&msg);
          break;
      }
    }
  }
/*
  else
  {
    print("Received bad message!\n");
    handle_dev_error(&msg);
  }
*/
}

int main( void )
{
  struct Device dev;
  int status;

  initVideo();

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

  dev.major = DEV_MAJOR;
  dev.numDevices = NUM_DEVICES;
  dev.dataBlkLen = 1;
  dev.type = CHAR_DEV;
  dev.cacheType = NO_CACHE;

  for( int i=0; i < 5; i++ )
  {
    status = registerDevice(DEVICE_NAME, strlen(DEVICE_NAME), &dev);

    if( status != 0 )
      __sleep( (i*i+1) * 500 );
    else
      break;
  }

  if( status != 0 )
    return 1;

  handleDevRequests();

  while(1)
    __pause();

  return 1;
}
