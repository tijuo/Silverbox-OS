#ifndef VIDEO_SERVER
#define VIDEO_SERVER

enum Colors { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GRAY,
              DRKGRAY, LTBLUE, LTGREEN, LTCYAN, LTRED, LTMAGENTA,
              YELLOW, WHITE, BLINK = 0x80, NO_BLINK = 0x00 };

#define ATTRIB( fore, back, blink ) (blink | ((back & 0x8) << 4) | fore );

#define VPUT_CHAR       0x00
#define VSET_CURSOR     0x01
#define VSET_CHAR_POS   0x02
#define VSHOW_CURSOR    0x03
#define VENABLE_CURSOR  0x04
#define VSCROLL         0x05
#define VSET_ATTRIB     0x06
#define VSET_MODE       0x07
#define VRESET          0x08
#define VCLEAR          0x09

#endif /* VIDEO_SERVER */
