#include <oslib.h>
#include <os/io.h>
#include <os/msg/message.h>
#include <drivers/video.h>
#include <stdlib.h>
#include <stdarg.h>
#include <os/dev_interface.h>
#include <os/services.h>
#include <string.h>
#include <stdio.h>
#include <os/syscalls.h>

#define MSG_TIMEOUT		    3000

#define VIDEO_RAM           0xB8000
#define BIOS_ROM            0xC0000

#define VIDEO_RAM_SIZE      (BIOS_ROM-VIDEO_RAM)

/* Video operations:
   Low-level:
   - putAttrAt( unsigned char attr, unsigned char pos )
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

#define SERVER_NAME 		    "video"
#define DEVICE_NAME		        "video"
#define DEV_MAJOR		        5
#define NUM_DEVICES		        1

#define HTAB_WIDTH 		        8

#define VID_PUTCH		        0
#define VID_GETCH		        1
#define VID_SETCURS		        2
#define VID_GET_CURS		    3
#define VID_SHOW_CURS		    4
#define VID_SCROLL		        5
#define VID_SET_ATTR		    6
#define VID_SET_MODE		    7
#define VID_RESET		        8
#define VID_CLEAR		        9
#define VID_ENAB_CURS		    10

#define SEND_EMPTY_RESPONSE(to, subj) \
({ \
    msg_t _response_message = { \
      .subject = subj, \
      .recipient = to, \
      .buffer = NULL, \
      .bufferLen = 0, \
      .flags = MSG_NOBLOCK \
  }; \
  sys_send(&_response_message); \
})

struct TextCharacter {
  unsigned char codePoint;

  union CharAttribute {
    struct {
      unsigned char fgColor : 4;
      unsigned char bgColor: 3;
      unsigned char blink : 1;
    };
    unsigned char value;
  } attrib;
};

typedef struct TextCharacter textchar_t;

char videoMsgBuffer[4096];

static unsigned int charXPos = 0;
static unsigned int charYPos = 0;
static unsigned int maxWidth = 80;
static unsigned int maxHeight = 25;
static unsigned int maxLines;
static int defaultAttrib = ATTRIB( GRAY, BLACK, NO_BLINK );
static textchar_t *cursorPtr = (textchar_t *)VIDEO_RAM;

//int putChar( char c, int xPos, int yPos );
int setCursor( unsigned int xPos, unsigned int yPos );
int setCursorPos( unsigned int pos );
int setColor( unsigned int color, unsigned int xPos, unsigned int yPos );
void clear_screen( unsigned int attrib );
int init_video( void );
int handleIoctl( int command, int numArgs, void *args );
inline int checkPos( unsigned int x, unsigned int y );
inline int setCharPos( unsigned int x, unsigned int y );

int handleDeviceRead( msg_t *msg );
int handleDeviceWrite(msg_t *msg);
int handle_dev_ioctl( msg_t *msg );
int handleDeviceError( msg_t *msg );

int addr_offset=0;

void scrollToLine( unsigned int line );
void scroll( unsigned int lines );

int setScreenOffset( unsigned int offset );

//void putChar(char c);
int putCharAt( char c, unsigned int pos );
int putCharAtXY( char c, unsigned int x, unsigned int y );


int putAttrAt( unsigned char attr, unsigned int pos );
int putAttrAtXY( unsigned char attr, unsigned int x, unsigned int y );

void _put_char(char c)
{
  cursorPtr->codePoint = (unsigned char)c;
  cursorPtr->attrib.value = defaultAttrib;
  cursorPtr++;

  if(cursorPtr >= (textchar_t *)VIDEO_RAM + maxLines * maxWidth)
  {
    cursorPtr = (textchar_t *)VIDEO_RAM;
    charXPos = 0;
    charYPos = 0;

    scrollToLine(0);
    setScreenOffset(0);
  }
  else
  {
    charXPos++;

    if(charXPos >= maxWidth)
    {
      charXPos = 0;
      charYPos++;

      if(charYPos >= maxHeight)
        charYPos = maxHeight - 1;

      scroll(1);
    }
  }

  setCursorPos((int)(cursorPtr - (textchar_t *)VIDEO_RAM));
}

int putAttrAt( unsigned char attr, unsigned int pos )
{
  if(pos >= VIDEO_RAM_SIZE / 2 )
    return -1;

  textchar_t *vidmem = (textchar_t *)VIDEO_RAM + pos;

  vidmem->attrib.value = attr;
  return 0;
}

int putAttrAtXY(unsigned char attr, unsigned int x, unsigned int y)
{
  return putAttrAt(attr, x + y*maxWidth);
}

int putCharAt( char c, unsigned int pos )
{
  if(pos >= VIDEO_RAM_SIZE / 2 )
    return -1;

  textchar_t *vidmem = (textchar_t *)VIDEO_RAM + pos;

  vidmem->codePoint = (unsigned char)c;
  return 0;
}

int putCharAtXY( char c, unsigned int x, unsigned int y )
{
  return putCharAt(c, y*maxWidth + x);
}

int setCursorPos( unsigned int pos )
{
  if(pos >= VIDEO_RAM_SIZE / 2 )
    return -1;

  outByte( CRTC_INDEX, CURSOR_LOC_LOW );
  outByte( CRTC_DATA, pos & 0xFF );
  outByte( CRTC_INDEX, CURSOR_LOC_HIGH );
  outByte( CRTC_DATA, pos >> 8 );
  return 0;
}

int setCursorXY( unsigned int x, unsigned int y )
{
  return setCursorPos(x + y * maxWidth);
}

int setScreenOffset( unsigned int offset )
{
  if(offset >= (unsigned int)((uintptr_t)BIOS_ROM - 2*maxWidth*maxHeight))
    return -1;

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
    data &= ~0x20;
  else
    data |= 0x20;

  outByte( CRTC_INDEX, CURSOR_START );
  outByte( CRTC_DATA, data );

  return 0;
}

/* ==========================================================================
   ========================================================================== */

void scroll( unsigned int lines )
{
  ptrdiff_t line = ((uintptr_t)cursorPtr - VIDEO_RAM - charXPos*sizeof(textchar_t)) / maxWidth;

  scrollToLine(line + lines);
}

void scrollToLine( unsigned int line )
{
  ptrdiff_t offset = (ptrdiff_t)((textchar_t *)VIDEO_RAM + line * maxWidth);

  offset = offset < 0 ? 0 : (offset > (maxLines-maxHeight)*maxWidth ? (maxLines-maxHeight)*maxWidth : offset);

  setScreenOffset( sizeof(textchar_t) * offset );
}

int checkPos( unsigned int x, unsigned int y )
{
  return (x >= maxWidth || y >= maxLines) ? -1 : 0;
}

int setCharPos( unsigned int x, unsigned int y )
{
  if( checkPos( x, y ) != 0 )
    return -1;

  charXPos = x;
  charYPos = y;
  cursorPtr = (textchar_t *)VIDEO_RAM + (y*maxWidth + x);

  return 0;
}

#define doNewline() \
  charXPos = 0; \
  charYPos++; \
  if( charYPos >= 25 ) scrollToLine( charYPos - 24 );

/* Go to nearest multiple of 8. */

#define doHTab() \
  charXPos += (HTAB_WIDTH - charXPos % HTAB_WIDTH); \
  charYPos = ((charXPos >= maxWidth) ? (charYPos + 1) : charYPos); \
  charXPos %= maxWidth;

#define doVTab() \
  charYPos = charYPos + 1;

#define doBacksp() \
  charYPos -= ((charXPos == 0 && charYPos > 0) ? 1 : 0); \
  charXPos = ((charXPos == 0) ? maxWidth - 1 : charXPos - 1);

/*
void putChar(char c)
{
  switch(c)
  {
    case '\n':
      charYPos++;
      charXPos = 0;
      break;
    case '\t':
      charXPos += TAB_WIDTH - (charXPos % TAB_WIDTH);
      break;
    case '\b':
      if(charXPos == 0)
      {
        if(charYPos)
        {
          charXPos = maxWidth - 1;
          charYPos--;
        }
      }
      else
        charXPos--;
      break;
    case '\f':
      charXPos = 0;
      charYPos = 0;
      break;
    case '\r':
      charXPos = 0;
      break;
    case '\v':
      charYPos++;
      break;
    case '\0':
    case '\a':
      break;
    default:
      _putChar(c);
      break;
  }

  if(charXPos == maxWidth)
  {
    charXPos = 0;
    charYPos++;
  }

  if(charYPos >= maxHeight)
  {
    if(charYPos >= maxLines)
    {
      charYPos = 0;
      charXPos = 0;

      scrollToLine(0);
    }
    else
    {
      scroll(maxHeight - charYPos);
      charYPos = maxHeight - 1;
    }
  }

  cursorPtr = charXPos + charYPos*maxWidth;

  if(isCursorOn && isCursorVisible)
    setCursor(charXPos, charYPos);
}
*/
int setCursor( unsigned int xPos, unsigned int yPos )
{
  if( checkPos( xPos, yPos ) == -1 )
    return -1;

  setCursorPos(xPos + yPos * maxWidth);
  return 0;
}

int setColor( unsigned int color, unsigned int xPos, unsigned int yPos )
{
  char *vidmem = (char *)COLOR_TXT_ADDR;

  if( checkPos( xPos, yPos ) == -1 )
    return -1;

  vidmem[(xPos + yPos * maxWidth) * 2 + 1] = color;

  return 0;
}


void clear_screen( unsigned int attrib )
{
  textchar_t c = {
      .codePoint = ' ',
      .attrib = { .value = attrib }
  };

  textchar_t *ptr = (cursorPtr - charXPos - charYPos*maxWidth);

  for( register size_t i=0; i < maxHeight*maxWidth; i++ )
    *ptr++ = c;
}

int init_video( void )
{
  int pmemDevice = 0x00010000;

  if(mapMem((addr_t)VIDEO_RAM, pmemDevice, (size_t)((uintptr_t)BIOS_ROM-(uintptr_t)VIDEO_RAM),
         VIDEO_RAM, MEM_FLG_NOCACHE) != 0) {
    fprintf(stderr, "Unable to map video RAM.\n");
    return -1;
  }

  defaultAttrib = ATTRIB( GRAY, BLACK, NO_BLINK );

//  changeIoPerm( CRTC_INDEX, CRTC_DATA, 1 );

  setCursor( 0, 0 );
  setCharPos( 0, 0 );

  clear_screen( defaultAttrib );

  return 0;
}

int handleDeviceRead(msg_t *msg)
{
  struct DeviceWriteResponse response = { .bytesTransferred = 0 };
  msg_t responseMsg = {
      .subject = RESPONSE_OK,
      .recipient = msg->sender,
      .buffer = &response,
      .bufferLen = sizeof response,
      .flags = MSG_NOBLOCK,
  };

  struct DeviceOpRequest *request = msg->buffer;
  size_t length = msg->bufferLen < request->length ? request->length : msg->bufferLen;

  switch(request->deviceMinor)
  {
    case 0:
      if(request->offset > BIOS_ROM-VIDEO_RAM)
      {
        response.bytesTransferred = 0;
        responseMsg.subject = DEVICE_NOMORE;
      }
      else
      {
        uintptr_t offset = VIDEO_RAM + request->offset;

        response.bytesTransferred = length < (size_t)(BIOS_ROM-offset) ? length : (size_t)(BIOS_ROM-offset);
        memcpy(msg->buffer, (void *)offset, response.bytesTransferred);
      }

      return sys_send(&responseMsg) == ESYS_OK ? 0 : -1;
    default:
      return handleDeviceError(msg);
  }
}

int handleDeviceWrite(msg_t *msg)
{
  struct DeviceWriteResponse response = { .bytesTransferred = 0 };
  msg_t responseMsg = {
      .subject = RESPONSE_OK,
      .recipient = msg->sender,
      .buffer = &response,
      .bufferLen = sizeof response,
      .flags = MSG_NOBLOCK,
  };

  struct DeviceOpRequest *request = (struct DeviceOpRequest *)msg->buffer;
  size_t length = msg->bufferLen < request->length ? request->length : msg->bufferLen;

  switch(request->deviceMinor)
  {
    case 0:
      if(request->offset > BIOS_ROM-VIDEO_RAM)
      {
        response.bytesTransferred = 0;
        responseMsg.subject = DEVICE_NOMORE;
      }
      else
      {
        uintptr_t offset = VIDEO_RAM + request->offset;

        response.bytesTransferred = length < (size_t)(BIOS_ROM-offset) ? length : (size_t)(BIOS_ROM-offset);
        memcpy((void *)offset, request->payload, response.bytesTransferred);
      }

      return sys_send(&responseMsg) == ESYS_OK ? 0 : -1;
    default:
      return handleDeviceError(msg);
  }
}

int handle_dev_ioctl( msg_t *msg )
{
  return handleDeviceError(msg);
}

int handleDeviceError( msg_t *msg )
{
  return SEND_EMPTY_RESPONSE(msg->sender, RESPONSE_ERROR) == ESYS_OK ? 0 : -1;
}

int main(void)
{
  int retval;

  maxLines = VIDEO_RAM_SIZE / (maxWidth * 2);

  if(init_video() != 0)
  {
    fprintf(stderr, "Unable to initialize video driver.\n");
    return EXIT_FAILURE;
  }

  size_t bufferLen = BIOS_ROM-VIDEO_RAM;
  void *buffer = malloc(bufferLen);

  if(!buffer)
  {
    fprintf(stderr, "Unable to allocate memory for buffer.\n");
    return EXIT_FAILURE;
  }

  if(registerName(SERVER_NAME) != 0)
  {
    fprintf(stderr, "Unable to register %s server.\n", SERVER_NAME);
    free(buffer);
    return EXIT_FAILURE;
  }

  while(true)
  {
    msg_t msg = {
        .sender = ANY_SENDER,
        .buffer = buffer,
        .bufferLen = bufferLen,
        .flags = 0
    };

    if(sys_receive(&msg) != ESYS_OK)
      continue;

    switch(msg.subject)
    {
      case DEVICE_READ:
        retval = handleDeviceRead(&msg);
        break;
      case DEVICE_WRITE:
        retval = handleDeviceWrite(&msg);
        break;
      case VSET_SCROLL:
      {
        struct VideoSetScrollRequest *scrollRequest = buffer;
        scrollToLine(scrollRequest->row);
        retval = SEND_EMPTY_RESPONSE(msg.sender, RESPONSE_OK) == ESYS_OK ? 0 : -1;
        break;
      }
      case VSET_CURSOR:
      {
        struct VideoSetCursorRequest *cursorRequest = buffer;
        int response = setCursor(cursorRequest->x, cursorRequest->y);
        retval = SEND_EMPTY_RESPONSE(msg.sender, response == 0 ? RESPONSE_OK : RESPONSE_FAIL) == ESYS_OK ? 0 : -1;
        break;
      }
      case VSHOW_CURSOR:
      {
        struct VideoEnableCursorRequest *cursorRequest = buffer;
        int response = showCursor(!!cursorRequest->enable);
        retval = SEND_EMPTY_RESPONSE(msg.sender, response == 0 ? RESPONSE_OK : RESPONSE_FAIL) == ESYS_OK ? 0 : -1;
        break;
      }
      case VCLEAR_SCREEN:
      {
        clear_screen(defaultAttrib);
        retval = SEND_EMPTY_RESPONSE(msg.sender, RESPONSE_OK) == ESYS_OK ? 0 : -1;
        break;
      }
      default:
        retval = handleDeviceError(&msg);
        break;
    }

    if(retval != 0)
      fprintf(stderr, "Error while handling request 0x%x.\n", msg.subject);
  }

  free(buffer);
  return EXIT_FAILURE;
}
