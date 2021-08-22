#ifndef CIRC_BUFFER_H
#define CIRC_BUFFER_H

#include <stdlib.h>

struct CircularBuffer {
  void *data;
  void *ptr;
  size_t unreadLen;
  size_t bufLen;
};

int initCircBuffer(struct CircularBuffer *buffer, void *data, size_t bufferSize);
size_t readCircBuffer(struct CircularBuffer *buffer, size_t bytes, void *outData);
size_t writeCircBuffer(struct CircularBuffer *buffer, size_t bytes, void *data);

#endif /* CIRC_BUFFER_H */
