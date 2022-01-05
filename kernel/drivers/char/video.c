#include <kernel/devmgr.h>
#include <kernel/bits.h>
#include <kernel/io.h>
#include <drivers/video.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <kernel/memory.h>

#define VIDEO_RAM   0xA0000
//#define VIDEO_RAM   0xB8000
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
static int video_get_maximum_scanline(void);

static uint8_t video_default_attrib = ATTRIB(LT_GREEN, BLACK, NO_BLINK);

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
         | (uint8_t)(end & CURSOR_SL_END_MASK);
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

static long int video_create(struct driver *this_obj, int id, const void *params, int flags) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_read(struct driver *this_obj, int minor, void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_write(struct driver *this_obj, int minor, const void *buf, size_t bytes, long int offset, int flags) {
  switch(minor) {
    case VIDEO_MINOR:
      return video_write_chars((char *)buf, bytes, (int)offset, video_default_attrib);
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_get(struct driver *this_obj, int id, va_list args) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_set(struct driver *this_obj, int id, va_list args) {
  switch(id) {
    case VIDEO_MINOR:
      return DEV_FAIL;
    default:
      return DEV_NO_EXIST;
  }
}

static long int video_destroy(struct driver *this_obj, int id) {
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
  // Sets Horizontal/Vertical sync polarity, 28 MHz clock
  out_port8(0x3C2, 0xC7);

  // Use 128 KB of video memory and enable text mode

  out_port8(0x3CE, 0x06);
  out_port8(0x3CF, in_port8(0x3CF) & ~0b00001101);

  /*
  // Disable CRTC protection (allows modifying CRTC registers 0-7)

  out_port8(CRTC_INDEX, 0x11);
  out_port8(CRTC_DATA, in_port8(CRTC_DATA) & ~0b10000000);

    // Enable 90x30 text mode by setting horizontal total/end register values to 90 char clocks
    // and vertical total/end register values to 480 scanlines.

  // Set Horizontal Total

  out_port8(CRTC_INDEX, 0x00);
  out_port8(CRTC_DATA, 85); // must be number of char clocks minus 5

  // Set Horizontal End
  out_port8(CRTC_INDEX, 0x01);
  out_port8(CRTC_DATA, 89); // must be number of char clocks minus 1

  // Set Vertical Total (lower 8 bits)

  out_port8(CRTC_INDEX, 0x06);
  out_port8(CRTC_DATA, 480 & 0xFFu);

  //Set Vertical End (lower 8 bits)

  out_port8(CRTC_INDEX, 0x12);
  out_port8(CRTC_DATA, 480 & 0xFFu);

  // Set bits 8 & 9 of Vertical Total and Vertical End

  out_port8(CRTC_INDEX, 0x07);
  out_port8(CRTC_DATA, (in_port8(CRTC_DATA) & 0b10011100) | 0b11);

  // Re-enable CRTC protection

  out_port8(CRTC_INDEX, 0x11);
  out_port8(CRTC_INDEX, in_port8(CRTC_DATA) | 0b10000000);
*/

  int maximum_sl = video_get_maximum_scanline();
  video_default_attrib = ATTRIB(WHITE, BLACK, NO_BLINK);
  video_set_screen_offset(0);
  video_set_cursor_scan(maximum_sl-1, maximum_sl);
  video_show_cursor(true);
  video_clear_screen(video_default_attrib);

  /*
  uint8_t data = in_port8(0x3CC) & ~(0x3u << 2);
  out_port8(0x3C2, data | (0x00u << 2));
*/

  char *lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Urna nec tincidunt praesent semper feugiat nibh. Lectus proin nibh nisl condimentum. Suspendisse faucibus interdum posuere lorem ipsum dolor sit amet. Tortor vitae purus faucibus ornare suspendisse. Tristique et egestas quis ipsum suspendisse ultrices. Auctor eu augue ut lectus arcu bibendum. Velit euismod in pellentesque massa placerat duis ultricies lacus. Gravida dictum fusce ut placerat orci nulla pellentesque dignissim enim. Rhoncus est pellentesque elit ullamcorper dignissim cras tincidunt lobortis feugiat. Fames ac turpis egestas maecenas pharetra convallis. Molestie a iaculis at erat pellentesque adipiscing commodo elit at. Urna condimentum mattis pellentesque id nibh tortor id aliquet lectus. Turpis egestas pretium aenean pharetra magna ac placerat. Volutpat ac tincidunt vitae semper. Consequat semper viverra nam libero justo laoreet sit. Faucibus et molestie ac feugiat sed lectus vestibulum. Magna ac placerat vestibulum lectus mauris ultrices. Turpis nunc eget lorem dolor. Quam viverra orci sagittis eu volutpat odio facilisis mauris. Nunc sed blandit libero volutpat sed cras. Pellentesque elit eget gravida cum sociis. Proin nibh nisl condimentum id venenatis a condimentum vitae sapien. Tempus urna et pharetra pharetra massa massa ultricies. Leo vel fringilla est ullamcorper eget. Eget mauris pharetra et ultrices neque. At augue eget arcu dictum varius duis at. Sed vulputate odio ut enim blandit volutpat. Viverra suspendisse potenti nullam ac tortor. Sed felis eget velit aliquet. Leo integer malesuada nunc vel risus commodo viverra maecenas accumsan. Commodo nulla facilisi nullam vehicula ipsum a. Odio morbi quis commodo odio aenean sed. Enim nulla aliquet porttitor lacus luctus accumsan tortor posuere. Ut pharetra sit amet aliquam id diam. Semper risus in hendrerit gravida rutrum quisque non.";

  int result = device_register(VIDEO_NAME, VIDEO_MAJOR, DEV_CHAR, 0, &video_callbacks);

  device_write(VIDEO_MAJOR, VIDEO_MINOR, lorem, kstrlen(lorem), 0, 0);

  return result;
}
