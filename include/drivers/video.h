#ifndef VIDEO_SERVER
#define VIDEO_SERVER

#define VIDEO_MINOR         0
#define VIDEO_NAME          "video"

enum Colors { BLACK=0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GRAY,
              DRK_GRAY, LT_BLUE, LT_GREEN, LT_CYAN, LT_RED, LT_MAGENTA,
              YELLOW, WHITE, BLINK = 0x80, NO_BLINK = 0x00 };

#define CRTC_INDEX		0x3D4
#define CRTC_DATA		0x3D5

#define GFX_MODE_ADDR		0xA0000
#define MONO_TXT_ADDR		0xB0000
#define COLOR_TXT_ADDR  	0xB8000

#define ATTRIB( fore, back, blink ) ((blink) | (((back) & 0x8) << 4) | ((fore) & 0xF) )

// Ioctl commands

#define VGET_INFO           0x80000000
#define VSET_CURSOR     	0x80000001
#define VSET_SCROLL         0x80000002
#define VSHOW_CURSOR    	0x80000003
#define VENABLE_CURSOR  	0x80000004
#define VGET_SCROLL         0x80000005
#define VGET_CURSOR
#define VSET_MODE       	0x08000007
#define VRESET          	0x80000008
#define VCLEAR_SCREEN       0x80000009

#define CURSOR_START		0x0A
#define CURSOR_END		    0x0B
#define START_ADDRESS_HIGH	0x0C
#define START_ADDRESS_LOW	0x0D
#define CURSOR_LOC_HIGH		0x0E
#define CURSOR_LOC_LOW		0x0F
#define VERTICAL_RET_START	0x10
#define VERTICAL_RET_END	0x11

struct VideoInfo
{
  unsigned int cursorX;
  unsigned int cursorY;
  int isCursorVisible;
  int numRows;
  int numCols;
  int mode;
};

struct VideoSetScrollRequest
{
  unsigned int row;
};

struct VideoSetCursorRequest
{
  unsigned int x;
  unsigned int y;
};

struct VideoEnableCursorRequest
{
  unsigned int enable;
};

int videoSetScroll(unsigned int lineOffset);
int videoSetCursor(unsigned int x, unsigned int y);
int videoGetInfo(void);
int videoEnableCursor(int enable);
int videoClearScreen(void);

#endif /* VIDEO_SERVER */
