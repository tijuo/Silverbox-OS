#include <kernel/devmgr.h>
#include <kernel/bits.h>
#include <kernel/io.h>
#include <drivers/video.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define VIDEO_RAM   0xB8000
#define BIOS_ROM    0xC0000

#define VIDEO_RAM_SIZE      (BIOS_ROM-VIDEO_RAM)

#define CRTC_INDEX    0x3D4
#define CRTC_DATA   0x3D5

#define GFX_MODE_ADDR   0xA0000
#define MONO_TXT_ADDR   0xB0000
#define COLOR_TXT_ADDR    0xB8000

#define ATTRIB( fore, back, blink ) ((blink) | (((back) & 0x8) << 4) | ((fore) & 0xF) )

#define MAXIMUM_SL          0x09
#define CURSOR_START        0x0A
#define CURSOR_END          0x0B
#define START_ADDRESS_HIGH  0x0C
#define START_ADDRESS_LOW   0x0D
#define CURSOR_LOC_HIGH     0x0E
#define CURSOR_LOC_LOW      0x0F
#define VERTICAL_RET_START  0x10
#define VERTICAL_RET_END    0x11

/// Bit 5 of Cursor Start Register (0x0A)
#define CURSOR_DISABLE    0x20

/// Cursor scanline start mask (Bits 0-4) of Cursor Start Register (0x0A)

#define CURSOR_SL_START_MASK  0x0F

/// Cursor scanline end mask (Bits 0-4) of Cursor En Register (0x0B)

#define CURSOR_SL_END_MASK    0x0F

#define MAXIMUM_SL_MASK       0x1F

/* Video operations:
   Low-level:
   - put_attr_at(unsigned char attr, unsigned char pos)
   - set_cursor_pos(int pos)
   - set_screen_offset(int offset)
   - show_cursor(bool val)

   Mid-level:
   - putChar(char c, int x, int y);
   - set_cursor(int x, int y);
   - set_char_pos(int x, int y);
   - scroll(int lines);
   - setAttr(int attrib)
   - setMode(int mode);
   - reset()
   - clear()
*/

#define SERVER_NAME         "video"
#define DEVICE_NAME           "video"
#define DEV_MAJOR           5
#define NUM_DEVICES           1

#define HTAB_WIDTH            8

#define VID_PUTCH           0
#define VID_GETCH           1
#define VID_SETCURS           2
#define VID_GET_CURS        3
#define VID_SHOW_CURS       4
#define VID_SCROLL            5
#define VID_SET_ATTR        6
#define VID_SET_MODE        7
#define VID_RESET           8
#define VID_CLEAR           9
#define VID_ENAB_CURS       10

#define FG_COLOR(c) ((c).attrib & 0x0Fu)
#define BG_COLOR(c) (((c).attrib >> 4) & 0x07u)
#define BLINK(c)  (((c).attrib >> 7) & 0x01u)

union video_text_character {
  struct {
    uint8_t code_point;
    uint8_t attrib;
  };

  uint16_t word;
};

typedef volatile union video_text_character video_textchar_t;

static void video_set_screen_offset(int offset);

static int video_put_char(char c, int pos, uint8_t attr);
static int video_write_chars(char *buf, size_t buf_len, int start_pos, uint8_t attr);
static void video_clear_screen(int attrib);

static void video_set_cursor_scan(int start, int end);
static void video_set_cursor_offset(int pos);
static void video_show_cursor(bool show);
static int video_get_maximum_scanline();

static uint8_t video_default_attrib = ATTRIB(GRAY, BLACK, NO_BLINK);

int video_put_char(char c, int pos, uint8_t attr) {
  video_textchar_t *vidmem = (video_textchar_t *)VIDEO_RAM + pos;

  if(pos < 0 || vidmem >= (video_textchar_t *)BIOS_ROM)
    return DEV_FAIL;
  else {
    vidmem->code_point = (unsigned char)c;
    vidmem->attrib = attr;

    return DEV_OK;
  }
}

int video_write_chars(char *buf, size_t buf_len, int start_pos, uint8_t attr) {
  video_textchar_t *vidmem = (video_textchar_t *)VIDEO_RAM + start_pos;
  size_t chars_written;

  if(start_pos < 0)
    return DEV_FAIL;

  for(chars_written=0;
      chars_written < buf_len && vidmem < (video_textchar_t *)BIOS_ROM;
      chars_written++, vidmem++, buf++) {
    vidmem->code_point = (uint8_t)*buf;
    vidmem->attrib = attr;
  }

  return chars_written;
}

void video_set_cursor_offset(int pos) {
  out_port8(CRTC_INDEX, CURSOR_LOC_LOW);
  out_port8(CRTC_DATA, (uint8_t)(pos & 0xFFu));
  out_port8(CRTC_INDEX, CURSOR_LOC_HIGH);
  out_port8(CRTC_DATA, (uint8_t)(pos >> 8));
}

void video_set_cursor_scan(int start, int end) {
  out_port8(CRTC_INDEX, CURSOR_START);
  uint8_t data = (in_port8(CRTC_DATA) & ~CURSOR_SL_START_MASK)
                 | (uint8_t)(start & CURSOR_SL_START_MASK);
  out_port8(CRTC_INDEX, CURSOR_START);
  out_port8(CRTC_DATA, data);

  out_port8(CRTC_INDEX, CURSOR_END);
  data = (in_port8(CRTC_DATA) & ~CURSOR_SL_END_MASK)
         | (uint8_t)(start & CURSOR_SL_END_MASK);
  out_port8(CRTC_INDEX, CURSOR_END);
  out_port8(CRTC_DATA, data);
}

void video_set_screen_offset(int offset) {
  if(offset < 0)
    offset = 0;

  out_port8(CRTC_INDEX, START_ADDRESS_LOW);
  out_port8(CRTC_DATA, (uint8_t)(offset & 0xFFu));
  out_port8(CRTC_INDEX, START_ADDRESS_HIGH);
  out_port8(CRTC_DATA, (uint8_t)(offset >> 8));
}

void video_show_cursor(bool show) {
  out_port8(CRTC_INDEX, CURSOR_START);
  uint8_t data = in_port8(CRTC_DATA);

  if(show)
    CLEAR_FLAG(data, CURSOR_DISABLE);
  else
    SET_FLAG(data, CURSOR_DISABLE);

  out_port8(CRTC_INDEX, CURSOR_START);
  out_port8(CRTC_DATA, data);
}

void video_clear_screen(int attrib) {
  video_textchar_t c = {
    .code_point = '\0',
    .attrib = (uint8_t)attrib
  };

  for(video_textchar_t *ptr = (video_textchar_t *)VIDEO_RAM; ptr < (video_textchar_t *)BIOS_ROM; ptr++)
    *ptr = c;
}

int video_get_maximum_scanline(void) {
  out_port8(CRTC_INDEX, MAXIMUM_SL);
  uint8_t data = in_port8(CRTC_DATA);

  return data & MAXIMUM_SL_MASK;
}

static long int video_create(int id, const void *params, int flags) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_read(int minor, void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case VIDEO_MINOR:
      return video_write_chars((char *)buf, bytes, (int)offset, video_default_attrib);
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_write(int minor, const void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_get(int id, ...) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_set(int id, ...) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_destroy(int id) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

struct driver_callbacks video_callbacks = {
  .create = video_create,
  .read = video_read,
  .write = video_write,
  .get = video_get,
  .set = video_set,
  .destroy = video_destroy
};

int video_init(void) {
  int maximum_sl = video_get_maximum_scanline();
  video_default_attrib = ATTRIB(GRAY, BLACK, NO_BLINK);
  video_set_screen_offset(0);
  video_set_cursor_scan(maximum_sl-1, maximum_sl);
  video_show_cursor(true);
  video_clear_screen(video_default_attrib);

  return device_register(VIDEO_NAME, VIDEO_MAJOR, DEV_CHAR, 0, &video_callbacks);
}
