#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
  if(stream == NULL) {
    errno = -EBADF;
    return -1;
  }
  else if(!stream->is_open) {
    errno = -ESTALE;
    return -1;
  }
  else {
    switch(mode) {
      case _IOFBF:
      case _IONBF:
        stream->buffer = buf;
        stream->buf_head = 0;
        stream->buf_tail = 0;
        stream->buffer_len = size;
        break;
      case _IOLBF:
        stream->buffer = NULL;
        stream->buffer_len = 0;
        break;
      default:
        stream->error = 1;
        errno = -EINVAL;
        return -1;
    }

    if(stream->write_req_buffer) {
      free(stream->write_req_buffer);
      stream->write_req_buffer = NULL;
    }

    if(stream->buffer) {
      stream->write_req_buffer = malloc(size + sizeof(struct DeviceOpRequest));

      if(!stream->write_req_buffer) {
        stream->error = 1;
        errno = -ENOMEM;
        return -1;
      }

      stream->write_req_pos = 0;
    }

    stream->user_buf = !!buf;
    stream->buf_mode = mode;
    return 0;
  }
}
