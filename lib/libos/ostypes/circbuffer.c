#include <os/ostypes/circbuffer.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int circular_buffer_init(CircularBuffer* buffer, void* data, size_t buffer_size)
{
    if(!data)
        return -1;

    buffer->data = buffer->ptr = data;
    buffer->buf_len = buffer_size;
    buffer->unread_len = 0;

    return 0;
}

int circular_buffer_create(CircularBuffer* buffer, size_t buffer_size)
{
    void *new_data = malloc(buffer_size);

    if(!new_data) {
        return -1;
    }

    return circular_buffer_init(buffer, new_data, buffer_size);
}

void circular_buffer_destroy(CircularBuffer* buffer) {
    free(buffer->data);
    buffer->data = NULL;
}

size_t circular_buffer_read(CircularBuffer* buffer, size_t bytes,
    void* out_data)
{
    size_t bytes_to_read = bytes > buffer->unread_len ? buffer->unread_len : bytes;
    size_t upper_size = (size_t)(((uintptr_t)buffer->data + buffer->buf_len)
        - (uintptr_t)buffer->ptr);

    if(bytes_to_read > upper_size) {
        memcpy(out_data, buffer->ptr, upper_size);
        memcpy((void*)((uintptr_t)out_data + upper_size), buffer->data,
            bytes_to_read - upper_size);
        buffer->ptr = (void*)((uintptr_t)buffer->data + (bytes_to_read - upper_size));
    } else {
        memcpy(out_data, buffer->ptr, bytes_to_read);
        buffer->ptr = (void*)((uintptr_t)buffer->ptr + bytes_to_read);
    }

    buffer->unread_len -= bytes_to_read;
    return bytes_to_read;
}

size_t circular_buffer_write(CircularBuffer* buffer, size_t bytes, void* data)
{
    size_t bytes_to_write =
        bytes > buffer->buf_len - buffer->unread_len ?
        buffer->buf_len - buffer->unread_len : bytes;
    uintptr_t ptr = (uintptr_t)buffer->ptr + buffer->unread_len;
    size_t upper_size = (size_t)((uintptr_t)buffer->data + buffer->buf_len - ptr);

    if(bytes_to_write > upper_size) {
        memcpy((void*)ptr, data, upper_size);
        memcpy(buffer->data, (void*)((uintptr_t)data + upper_size),
            bytes_to_write - upper_size);
    } else {
        memcpy((void*)ptr, data, bytes_to_write);
    }

    buffer->unread_len += bytes_to_write;
    return bytes_to_write;
}
