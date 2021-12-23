#include <drivers/console.h>
#include <os/dev_interface.h>
#include <drivers/keyboard.h>
#include <os/ostypes/circbuffer.h>
#include <os/spinlock.h>
#include <drivers/video.h>
#include <os/services.h>
#include <string.h>
#include <stdio.h>

#define NUM_CONSOLES                8
#define KB_BUFFER_SIZE              1024
#define DEFAULT_ROWS                25
#define DEFAULT_COLS                80
#define MAX_ROWS                    250
#define VIDEO_BUFFER_SIZE           (DEFAULT_COLS * MAX_ROWS)
#define TAB_WIDTH                   4

#define VIDEO_OFFSET(console, x, y) ((x) + (console)->cols * (y))
#define CURSOR_OFFSET(console)      VIDEO_OFFSET(console, console->cursor_x, console->row_offset+console->cursor_y)
#define SCREEN_OFFSET(console)      VIDEO_OFFSET(console, console->cursor_x, console->cursor_y)

struct console {
  bool is_echo_chars;
  LOCKED_RES(char *, video_buffer);
  unsigned int row_offset;
  unsigned int rows_per_screen;
  unsigned int rows;
  unsigned int cols;
  unsigned int cursor_x;
  unsigned int cursor_y;
  size_t video_ram_offset;
};

static int init_console(size_t index);
static int print_char(struct console *console, int c);
static int print_chars(struct console *console, char *str, size_t len);
static void keyboard_listener(void);

int handleDeviceRead(msg_t *msg);
int handleDeviceWrite(msg_t *msg);

LOCKED_RES(struct circular_buffer, kb_buffer);
LOCKED_RES(size_t, current_console);
struct console consoles[NUM_CONSOLES];

tid_t videoTid;
char *printBuf;

int init_console(size_t index)
{
  if(index >= NUM_CONSOLES)
    return -1;

  struct console *console = &consoles[index];

  console->video_buffer = malloc(VIDEO_BUFFER_SIZE);

  if(!console->video_buffer)
    return -1;

  SPINLOCK_INIT(console->video_buffer);
  console->is_echo_chars = true;
  console->row_offset = 0;
  console->rows = MAX_ROWS;
  console->cols = DEFAULT_COLS;
  console->rows_per_screen = DEFAULT_ROWS;
  console->cursor_x = 0;
  console->cursor_y = 0;
  console->video_ram_offset = index * (console->cols * console->rows_per_screen);

  return 0;
}

int print_char(struct console *console, int c)
{
  return print_chars(console, (char *)&c, 1);
}

#define WRITE_TO_VIDEO \
    SPINLOCK_RELEASE(console->video_buffer); \
    deviceWrite(videoTid, devRequest, &w); \
    if(w == -1) return -1; \
    charsWritten += w; \
    SPINLOCK_WAIT(console->video_buffer);

int print_chars(struct console *console, char *str, size_t len)
{
  if(!len || !str)
    return -1;

  struct DeviceOpRequest *devRequest = (struct DeviceOpRequest *)printBuf;

  devRequest->deviceMinor = VIDEO_MINOR;
  devRequest->offset = CURSOR_OFFSET(console);
  devRequest->flags = 0;

  SPINLOCK_WAIT(console->video_buffer);

  size_t currentRow = console->row_offset;
  size_t charsWritten = 0;
  size_t w;

  for(size_t i=0; i < len; i++)
  {
    if(str[i] < 32)
    {
      switch(str[i])
      {
        case '\n':
          console->cursor_y++;

          if(console->cursor_y >= console->rows_per_screen)
          {
            size_t lineShift = 1 + console->cursor_y - console->rows_per_screen;
            console->cursor_y = console->rows_per_screen - 1;

            if(console->row_offset + lineShift >= console->rows - console->rows_per_screen)
            {
              console->row_offset = 0;
              console->cursor_y = 0;

              devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

              memcpy(devRequest->payload, &console->video_buffer[devRequest->offset], devRequest->length);

              WRITE_TO_VIDEO

              devRequest->offset = 0;
            }
            else
              console->row_offset += lineShift;
          }
          break;
        case '\r':
          console->cursor_x = 0;
          break;
        case '\f':
        case '\v':
          console->cursor_y++;

          if(console->cursor_y >= console->rows_per_screen)
          {
            console->cursor_y = console->rows_per_screen - 1;
            console->row_offset++;

            if(console->row_offset >= console->rows)
            {
              console->row_offset = 0;
              console->cursor_y = 0;

              devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

              memcpy(devRequest->payload, &console->video_buffer[devRequest->offset], devRequest->length);

              WRITE_TO_VIDEO

              devRequest->offset = 0;
            }
          }
          break;
        case '\t':
          console->cursor_x += TAB_WIDTH - (console->cursor_x % TAB_WIDTH);

          if(console->cursor_x >= console->cols)
            console->cursor_x = 0;
          break;
        case '\b':
          if(console->cursor_x == 0)
          {
            if(console->cursor_y == 0)
            {
              if(console->row_offset != 0)
              {
                console->row_offset -= 1;
                console->cursor_x = console->cols - 1;
              }
            }
            else
            {
              console->cursor_x = console->cols - 1;
              console->cursor_y--;
            }
          }
          else
            console->cursor_x--;
          break;
        default:
          break;
      }
    }
    else
    {
      console->video_buffer[CURSOR_OFFSET(console)] = str[i];
      console->cursor_x++;

      if(console->cursor_x >= console->cols)
      {
        console->cursor_x = 0;
        console->cursor_y++;

        if(console->cursor_y >= console->rows_per_screen)
        {
          size_t lineShift = 1 + console->cursor_y - console->rows_per_screen;
          console->cursor_y = console->rows_per_screen - 1;

          if(console->row_offset + lineShift >= console->rows - console->rows_per_screen)
          {
            console->row_offset = 0;
            console->cursor_y = 0;

            devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

            memcpy(devRequest->payload, &console->video_buffer[devRequest->offset], devRequest->length);

            WRITE_TO_VIDEO

            devRequest->offset = 0;
          }
          else
            console->row_offset += lineShift;
        }
      }
    }
  }

  devRequest->length = CURSOR_OFFSET(console) - devRequest->offset;

  if(devRequest->length)
  {
    memcpy(devRequest->payload, &console->video_buffer[devRequest->offset], devRequest->length);

    WRITE_TO_VIDEO
  }

  if(currentRow != console->row_offset)
    videoSetScroll(console->row_offset);

  videoSetCursor(console->cursor_x, console->cursor_y);

  SPINLOCK_RELEASE(console->video_buffer);

  return charsWritten;
}

// The keyboard listener merely echoes the characters that are typed to the video

void keyboard_listener(void)
{
  char key;
  tid_t keyboardTid;
  struct DeviceOpRequest keyboardRequest = {
      .deviceMinor = KEYBOARD_MINOR,
      .offset = 0,
      .length = 1,
      .flags = 0
  };
  struct console *console;

  keyboardTid = lookupName(KEYBOARD_NAME);

  while(1)
  {
    if(deviceRead(keyboardTid, &keyboardRequest, &key, NULL) == 0)
    {
      int writeResult;
      int is_echo_chars;

      SPINLOCK_WAIT(kb_buffer);

      writeResult = writeCircBuffer(&kb_buffer, sizeof(char), &key);

      SPINLOCK_RELEASE(kb_buffer);

      if(writeResult != 0)
        continue;

      SPINLOCK_WAIT(current_console);
      console = &consoles[current_console];

      SPINLOCK_WAIT(console->video_buffer);
      is_echo_chars = console->is_echo_chars;
      SPINLOCK_RELEASE(console->video_buffer);

      if(is_echo_chars)
        print_char(console, (int)key);

      SPINLOCK_RELEASE(current_console);
    }
  }
}

int handleDeviceRead(msg_t *msg)
{
  struct DeviceOpRequest *request = (struct DeviceOpRequest *)msg->buffer;

  if(request->deviceMinor != 0)
    return -1;

  return -1;
}

int handleDeviceWrite(msg_t *msg)
{
  struct DeviceOpRequest *request = (struct DeviceOpRequest *)msg->buffer;

  if(request->deviceMinor >= NUM_CONSOLES)
    return -1;
  else
  {
    uint8_t response[sizeof(msg_t) + sizeof(struct DeviceWriteResponse)];
    msg_t *responseMsg = (msg_t *)response;
    struct DeviceWriteResponse *responsePayload = (struct DeviceWriteResponse *)(responseMsg + 1);

    int result = print_chars(&consoles[request->deviceMinor], (char *)request->payload, request->length);

    if(result != -1)
      responsePayload->bytesTransferred = (size_t)result;

    responseMsg->subject = result == -1 ? RESPONSE_ERROR : RESPONSE_OK;
    responseMsg->flags = MSG_NOBLOCK;
    responseMsg->recipient = msg->sender;
    responseMsg->buffer = result != -1 ? (void *)response : NULL;
    responseMsg->bufferLen = result != -1 ? sizeof response : 0;

    return sys_send(responseMsg) == ESYS_OK ? 0 : -1;
  }
}

int main(void)
{
  printBuf = malloc(VIDEO_BUFFER_SIZE + sizeof(struct DeviceOpRequest));

  if(!printBuf)
  {
    fprintf(stderr, "Unable to allocate memory for print buffer.\n");
    return EXIT_FAILURE;
  }

  for(size_t i=0; i < NUM_CONSOLES; i++)
  {
    if(init_console(i) != 0)
    {
      fprintf(stderr, "Unable to initialize console %zu.\n", i);
      return EXIT_FAILURE;
    }
  }

  if(createCircBuffer(&kb_buffer, KB_BUFFER_SIZE * sizeof(char)) != 0)
  {
    fprintf(stderr, "Unable to allocate memory for keyboard buffer.\n");
    return EXIT_FAILURE;
  }

  videoTid = lookupName(VIDEO_NAME);

  //spawnThread(&keyboard_listener);

  if(registerName(console_NAME) == 0)
    fprintf(stderr, "%s registered successfully.\n", console_NAME);
  else
  {
    fprintf(stderr, "Unable to register %s.\n", console_NAME);
    return EXIT_FAILURE;
  }

  while(1)
  {
    msg_t msg = {
        .sender = ANY_SENDER,
        .buffer = printBuf,
        .bufferLen = VIDEO_BUFFER_SIZE + sizeof(struct DeviceOpRequest),
        .flags = 0,
    };

    if(sys_receive(&msg) != ESYS_OK)
      continue;

    switch(msg.subject)
    {
      case DEVICE_READ:
        handleDeviceRead(&msg);
        break;
      case DEVICE_WRITE:
        handleDeviceWrite(&msg);
        break;
      default:
        break;
    }
  }

  free(printBuf);
  return EXIT_FAILURE;
}
