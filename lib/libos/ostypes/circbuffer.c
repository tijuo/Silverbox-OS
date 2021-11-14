#include <os/ostypes/circbuffer.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int initCircBuffer(struct CircularBuffer *buffer, void *data, size_t bufferSize)
{
  if(!buffer || !data)
    return -1;

  buffer->data = buffer->ptr = data;
  buffer->bufLen = bufferSize;
  buffer->unreadLen = 0;

  return 0;
}

int createCircBuffer(struct CircularBuffer *buffer, size_t bufferSize) {
  return initCircBuffer(buffer, malloc(bufferSize), bufferSize);
}

size_t readCircBuffer(struct CircularBuffer *buffer, size_t bytes,
                      void *outData)
{
  size_t bytesToRead = bytes > buffer->unreadLen ? buffer->unreadLen : bytes;
  size_t upperSize = (size_t)(((uintptr_t)buffer->data + buffer->bufLen)
      - (uintptr_t)buffer->ptr);

  if(bytesToRead > upperSize) {
    memcpy(outData, buffer->ptr, upperSize);
    memcpy((void*)((uintptr_t)outData + upperSize), buffer->data,
           bytesToRead - upperSize);
    buffer->ptr = (void*)((uintptr_t)buffer->data + (bytesToRead - upperSize));
  }
  else {
    memcpy(outData, buffer->ptr, bytesToRead);
    buffer->ptr = (void*)((uintptr_t)buffer->ptr + bytesToRead);
  }

  buffer->unreadLen -= bytesToRead;
  return bytesToRead;
}

size_t writeCircBuffer(struct CircularBuffer *buffer, size_t bytes, void *data)
{
  size_t bytesToWrite =
      bytes > buffer->bufLen - buffer->unreadLen ?
          buffer->bufLen - buffer->unreadLen : bytes;
  uintptr_t ptr = (uintptr_t)buffer->ptr + buffer->unreadLen;
  size_t upperSize = (size_t)((uintptr_t)buffer->data + buffer->bufLen - ptr);

  if(bytesToWrite > upperSize) {
    memcpy((void*)ptr, data, upperSize);
    memcpy(buffer->data, (void*)((uintptr_t)data + upperSize),
           bytesToWrite - upperSize);
  }
  else
    memcpy((void*)ptr, data, bytesToWrite);

  buffer->unreadLen += bytesToWrite;
  return bytesToWrite;
}
