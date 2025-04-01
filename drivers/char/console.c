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
#define CURSOR_OFFSET(console)      VIDEO_OFFSET(console, console->cursorX, console->rowOffset+console->cursorY)
#define SCREEN_OFFSET(console)      VIDEO_OFFSET(console, console->cursorX, console->cursorY)

struct Console {
  bool isEcho;
  LOCKED_RES(char *, videoBuffer);
  unsigned int rowOffset;
  unsigned int rowsPerScreen;
  unsigned int rows;
  unsigned int cols;
  unsigned int cursorX;
  unsigned int cursorY;
  size_t videoRamOffset;
};

static int initConsole(size_t index);
static int printChar(struct Console *console, int c);
static int printChars(struct Console *console, char *str, size_t len);
static void keyboardListener(void);

int handleDeviceRead(msg_t *msg);
int handleDeviceWrite(msg_t *msg);

LOCKED_RES(CircularBuffer, kbBuffer);
LOCKED_RES(size_t, currentConsole);
struct Console consoles[NUM_CONSOLES];

tid_t videoTid;
char *printBuf;

int initConsole(size_t index)
{
  if(index >= NUM_CONSOLES)
    return -1;

  struct Console *console = &consoles[index];

  console->videoBuffer = malloc(VIDEO_BUFFER_SIZE);

  if(!console->videoBuffer)
    return -1;

  SPINLOCK_INIT(console->videoBuffer);
  console->isEcho = true;
  console->rowOffset = 0;
  console->rows = MAX_ROWS;
  console->cols = DEFAULT_COLS;
  console->rowsPerScreen = DEFAULT_ROWS;
  console->cursorX = 0;
  console->cursorY = 0;
  console->videoRamOffset = index * (console->cols * console->rowsPerScreen);

  return 0;
}

int printChar(struct Console *console, int c)
{
  return printChars(console, (char *)&c, 1);
}

#define WRITE_TO_VIDEO \
    SPINLOCK_RELEASE(console->videoBuffer); \
    deviceWrite(videoTid, devRequest, &w); \
    if(w == -1) return -1; \
    charsWritten += w; \
    SPINLOCK_WAIT(console->videoBuffer);

int printChars(struct Console *console, char *str, size_t len)
{
  if(!len || !str)
    return -1;

  struct DeviceOpRequest *devRequest = (struct DeviceOpRequest *)printBuf;

  devRequest->deviceMinor = VIDEO_MINOR;
  devRequest->offset = CURSOR_OFFSET(console);
  devRequest->flags = 0;

  SPINLOCK_WAIT(console->videoBuffer);

  size_t currentRow = console->rowOffset;
  size_t charsWritten = 0;
  size_t w;

  for(size_t i=0; i < len; i++)
  {
    if(str[i] < 32)
    {
      switch(str[i])
      {
        case '\n':
          console->cursorY++;

          if(console->cursorY >= console->rowsPerScreen)
          {
            size_t lineShift = 1 + console->cursorY - console->rowsPerScreen;
            console->cursorY = console->rowsPerScreen - 1;

            if(console->rowOffset + lineShift >= console->rows - console->rowsPerScreen)
            {
              console->rowOffset = 0;
              console->cursorY = 0;

              devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

              memcpy(devRequest->payload, &console->videoBuffer[devRequest->offset], devRequest->length);

              WRITE_TO_VIDEO

              devRequest->offset = 0;
            }
            else
              console->rowOffset += lineShift;
          }
          break;
        case '\r':
          console->cursorX = 0;
          break;
        case '\f':
        case '\v':
          console->cursorY++;

          if(console->cursorY >= console->rowsPerScreen)
          {
            console->cursorY = console->rowsPerScreen - 1;
            console->rowOffset++;

            if(console->rowOffset >= console->rows)
            {
              console->rowOffset = 0;
              console->cursorY = 0;

              devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

              memcpy(devRequest->payload, &console->videoBuffer[devRequest->offset], devRequest->length);

              WRITE_TO_VIDEO

              devRequest->offset = 0;
            }
          }
          break;
        case '\t':
          console->cursorX += TAB_WIDTH - (console->cursorX % TAB_WIDTH);

          if(console->cursorX >= console->cols)
            console->cursorX = 0;
          break;
        case '\b':
          if(console->cursorX == 0)
          {
            if(console->cursorY == 0)
            {
              if(console->rowOffset != 0)
              {
                console->rowOffset -= 1;
                console->cursorX = console->cols - 1;
              }
            }
            else
            {
              console->cursorX = console->cols - 1;
              console->cursorY--;
            }
          }
          else
            console->cursorX--;
          break;
        default:
          break;
      }
    }
    else
    {
      console->videoBuffer[CURSOR_OFFSET(console)] = str[i];
      console->cursorX++;

      if(console->cursorX >= console->cols)
      {
        console->cursorX = 0;
        console->cursorY++;

        if(console->cursorY >= console->rowsPerScreen)
        {
          size_t lineShift = 1 + console->cursorY - console->rowsPerScreen;
          console->cursorY = console->rowsPerScreen - 1;

          if(console->rowOffset + lineShift >= console->rows - console->rowsPerScreen)
          {
            console->rowOffset = 0;
            console->cursorY = 0;

            devRequest->length = VIDEO_BUFFER_SIZE - devRequest->offset;

            memcpy(devRequest->payload, &console->videoBuffer[devRequest->offset], devRequest->length);

            WRITE_TO_VIDEO

            devRequest->offset = 0;
          }
          else
            console->rowOffset += lineShift;
        }
      }
    }
  }

  devRequest->length = CURSOR_OFFSET(console) - devRequest->offset;

  if(devRequest->length)
  {
    memcpy(devRequest->payload, &console->videoBuffer[devRequest->offset], devRequest->length);

    WRITE_TO_VIDEO
  }

  if(currentRow != console->rowOffset)
    videoSetScroll(console->rowOffset);

  videoSetCursor(console->cursorX, console->cursorY);

  SPINLOCK_RELEASE(console->videoBuffer);

  return charsWritten;
}

// The keyboard listener merely echoes the characters that are typed to the video

void keyboardListener(void)
{
  char key;
  tid_t keyboardTid;
  struct DeviceOpRequest keyboardRequest = {
      .deviceMinor = KEYBOARD_MINOR,
      .offset = 0,
      .length = 1,
      .flags = 0
  };
  struct Console *console;

  keyboardTid = lookupName(KEYBOARD_NAME);

  while(1)
  {
    if(deviceRead(keyboardTid, &keyboardRequest, &key, NULL) == 0)
    {
      int writeResult;
      int isEcho;

      SPINLOCK_WAIT(kbBuffer);

      writeResult = circular_buffer_write(&kbBuffer, sizeof(char), &key);

      SPINLOCK_RELEASE(kbBuffer);

      if(writeResult != 0)
        continue;

      SPINLOCK_WAIT(currentConsole);
      console = &consoles[currentConsole];

      SPINLOCK_WAIT(console->videoBuffer);
      isEcho = console->isEcho;
      SPINLOCK_RELEASE(console->videoBuffer);

      if(isEcho)
        printChar(console, (int)key);

      SPINLOCK_RELEASE(currentConsole);
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
    msg_t *response_msg = (msg_t *)response;
    struct DeviceWriteResponse *responsePayload = (struct DeviceWriteResponse *)(response_msg + 1);

    int result = printChars(&consoles[request->deviceMinor], (char *)request->payload, request->length);

    if(result != -1)
      responsePayload->bytesTransferred = (size_t)result;

    response_msg->subject = result == -1 ? RESPONSE_ERROR : RESPONSE_OK;
    response_msg->flags = MSG_NOBLOCK;
    response_msg->recipient = msg->sender;
    response_msg->buffer = result != -1 ? (void *)response : NULL;
    response_msg->bufferLen = result != -1 ? sizeof response : 0;

    return sys_send(response_msg) == ESYS_OK ? 0 : -1;
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
    if(initConsole(i) != 0)
    {
      fprintf(stderr, "Unable to initialize console %zu.\n", i);
      return EXIT_FAILURE;
    }
  }

  if(circular_buffer_create(&kbBuffer, KB_BUFFER_SIZE * sizeof(char)) != 0)
  {
    fprintf(stderr, "Unable to allocate memory for keyboard buffer.\n");
    return EXIT_FAILURE;
  }

  videoTid = lookupName(VIDEO_NAME);

  //spawnThread(&keyboardListener);

  if(registerName(CONSOLE_NAME) == 0)
    fprintf(stderr, "%s registered successfully.\n", CONSOLE_NAME);
  else
  {
    fprintf(stderr, "Unable to register %s.\n", CONSOLE_NAME);
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
