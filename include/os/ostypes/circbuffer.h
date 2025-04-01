#ifndef CIRC_BUFFER_H
#define CIRC_BUFFER_H

#include <stdlib.h>

typedef struct {
    void* data;
    void* ptr;
    size_t unread_len;
    size_t buf_len;
} CircularBuffer;

int circular_buffer_init(CircularBuffer* buffer, void* data, size_t bufferSize);
int circular_buffer_create(CircularBuffer* buffer, size_t bufferSize);
size_t circular_buffer_read(CircularBuffer* buffer, size_t bytes, void* outData);
size_t circular_buffer_write(CircularBuffer* buffer, size_t bytes, void* data);
void circular_buffer_destroy(CircularBuffer* buffer);

#endif /* CIRC_BUFFER_H */
