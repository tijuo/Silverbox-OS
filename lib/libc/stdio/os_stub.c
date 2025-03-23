#include <stdio.h>
#include <string.h>
#include <os/dev_interface.h>

int _flushToFile(FILE *stream);
int _writeCharToFile(FILE *stream, int c);
int getFileAttributes( const char *path, struct FileAttributes *attrib );

int getFileAttributes( const char *path, struct FileAttributes *attrib ) {
    return -1;
}

int _writeCharToFile(FILE *stream, int c) {
  if(stream->is_device) {
    if(stream->dev_minor == DEBUG_DEVICE)
      asm("out %%al, %%dx\n" :: "d"(0xE9), "a"((unsigned char)c));
    else {
      stream->write_req_buffer->payload[0] = (uint8_t)c;
      stream->write_req_buffer->deviceMinor = stream->dev_minor;
      stream->write_req_buffer->length = 1;
      stream->write_req_buffer->offset = 0;
      stream->write_req_buffer->flags =
          stream->access == ACCESS_AP ? DEVF_APPEND : 0;
/*
      if(deviceWrite(stream->dev_server, stream->write_req_buffer, NULL) == -1)
        return -1;
        */
    }

    return 0;
  }

  return -1;
}

int _flushToFile(FILE *stream) {
  if(stream->is_device) {
    /*
     * If the circular buffer's head equals its tail, then it's either empty or full.
     * If it's full, then copy one byte to the write request and mark the circular buffer
     * as not full.
     */

    if(stream->is_buf_full) {
      if(stream->dev_minor == DEBUG_DEVICE)
        asm("out %%al, %%dx\n" :: "d"(0xE9), "a"((unsigned char)stream->buffer[stream->buf_tail]));
      else if(stream->user_buf)
        stream->write_req_buffer->payload[stream->write_req_pos++] = stream
          ->buffer[stream->buf_tail];

      stream->buf_tail = (stream->buf_tail + 1) % stream->buffer_len;
      stream->is_buf_full = 0;
    }

    /*
     * Write the buffer's content to the write request.
     */

    while(stream->buf_tail != stream->buf_head) {
      if(stream->dev_minor == DEBUG_DEVICE) {
        asm("out %%al, %%dx\n" :: "d"(0xE9), "a"((unsigned char)stream->buffer[stream->buf_tail]));
        stream->buf_tail = (stream->buf_tail + 1) % stream->buffer_len;
      }
      else {
        if(stream->user_buf) {
          size_t bytes;

          if(stream->buf_head < stream->buf_tail) {
            bytes = stream->buffer_len - stream->buf_tail;
            memcpy(&stream->write_req_buffer->payload[stream->write_req_pos],
                   &stream->buffer[stream->buf_tail], bytes);
            stream->buf_tail = 0;
            stream->write_req_pos += bytes;
          }

          bytes = stream->buf_head - stream->buf_tail;

          if(bytes) {
            memcpy(&stream->write_req_buffer->payload[stream->write_req_pos],
                   &stream->buffer[stream->buf_tail], bytes);
            stream->buf_tail += bytes;
            stream->write_req_pos += bytes;
          }
        }
      }
    }

    // Send the write request to the device server

    if(stream->dev_minor != DEBUG_DEVICE) {
      stream->write_req_buffer->deviceMinor = stream->dev_minor;
      stream->write_req_buffer->length = stream->write_req_pos;
      stream->write_req_buffer->offset = stream->file_pos;
      stream->write_req_buffer->flags =
          stream->access == ACCESS_AP ? DEVF_APPEND : 0;
/*
      if(deviceWrite(stream->dev_server, stream->write_req_buffer, NULL) == -1)
        return -1;
        */
    }

    return 0;
  }
  else {
    // TODO: to be implemented for non-device files
    return -1;
  }
}
