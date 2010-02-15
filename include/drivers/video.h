#ifndef VIDEO_SERVER
#define VIDEO_SERVER

enum Colors { BLACK=0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GRAY,
              DRK_GRAY, LT_BLUE, LT_GREEN, LT_CYAN, LT_RED, LT_MAGENTA,
              YELLOW, WHITE, BLINK = 0x80, NO_BLINK = 0x00 };

#define CRTC_INDEX		0x3D4
#define CRTC_DATA		0x3D5

#define GFX_MODE_ADDR		0xA0000
#define MONO_TXT_ADDR		0xB0000
#define COLOR_TXT_ADDR  	0xB8000

#define ATTRIB( fore, back, blink ) (blink | ((back & 0x8) << 4) | fore );

#define VPUT_CHAR       	0x00
#define VSET_CURSOR     	0x01
#define VSET_CHAR_POS   	0x02
#define VSHOW_CURSOR    	0x03
#define VENABLE_CURSOR  	0x04
#define VSCROLL         	0x05
#define VSET_ATTRIB     	0x06
#define VSET_MODE       	0x07
#define VRESET          	0x08
#define VCLEAR          	0x09

#define CURSOR_START		0x0A
#define CURSOR_END		0x0B
#define START_ADDRESS_HIGH	0x0C
#define START_ADDRESS_LOW	0x0D
#define CURSOR_LOC_HIGH		0x0E
#define CURSOR_LOC_LOW		0x0F
#define VERTICAL_RET_START	0x10
#define VERTICAL_RET_END	0x11

#endif /* VIDEO_SERVER */
