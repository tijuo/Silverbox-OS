#ifndef DRIVERS_VIDEO_H
#define DRIVERS_VIDEO_H

#define VIDEO_MAJOR         3
#define VIDEO_MINOR         0
#define VIDEO_NAME          "video"

enum video_colors { BLACK=0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GRAY,
                    DRK_GRAY, LT_BLUE, LT_GREEN, LT_CYAN, LT_RED, LT_MAGENTA,
                    YELLOW, WHITE, BLINK = 0x80, NO_BLINK = 0x00
                  };

int video_init(void);

#endif /* DRIVERS_VIDEO_H */
