#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF			((int)-1)
#define BUFSIZ			1024
#define FILENAME_MAX    256
#define FOPEN_MAX		64

// Buffer modes
#define _IOFBF			1		// Fully-buffered
#define _IOLBF			2		// Line-buffered
#define _IONBF			3		// Non-buffered

#define NO_ORI			0		// No orientation
#define BYTE_ORI		1		// Byte oriented
#define WIDE_ORI		2		// Wide oriented

#define L_tmpnam

#ifndef NULL
  #define NULL 0
#endif /* NULL */

#define SEEK_CUR	        0
#define SEEK_END	        1
#define SEEK_SET	        2

#define TMP_MAX		        30

#define ACCESS_RD		    1
#define ACCESS_WR		    2
#define ACCESS_AP           3

struct _STDIO_FILE
{
  unsigned int is_open         : 1;
  unsigned int is_binary       : 1;
  unsigned int access          : 2;
  unsigned int buf_mode        : 2;
  unsigned int eof             : 1;
  unsigned int error           : 1;
  unsigned int user_buf        : 1;   // 1 if 'buffer' has been supplied by the user
  unsigned int is_device       : 1;
  unsigned int orientation     : 2;
  unsigned int is_buf_full     : 1;   // 1 if the buffer cannot be written to (because it's full)
  unsigned int is_update       : 1;   // 1 if the fopen() mode string included a '+'
                                      // in update mode, buffers are flushed/repositioned immediately
                                      // before a write if reads have been performed. buffers are
                                      // repositioned immediately before a read if writes have been
                                      // performed
  unsigned int performed_read  : 1;   // 1 if the last operation performed was a read
  unsigned int performed_write : 1;   // 1 if the last operation performed was a write

  char filename[FILENAME_MAX];
  size_t filename_len;

  uint64_t file_pos;            // current position in the file

  uint64_t file_len;            // length of the file in bytes

  unsigned short int   dev_minor; // the device corresponding to the file
  tid_t dev_server;             // tid of the device server
  struct DeviceOpRequest *write_req_buffer; // the buffer that will be used to send the write request to the device server

  size_t write_req_pos;         // current offset of the write request (only valid if user_buf = 1)

  char *buffer;                 // the buffer used for reads/writes. if user_buf = 0, then
                                // buffer is a pointer into write_req_pos
                                // (i.e. buffer = &stream->write_req_pos[sizeof (struct DeviceOpRequest)])

  size_t buffer_len;            // the size of the buffer

  size_t buf_head;              // the position of the write cursor. Writing to the buffer advances
                                // the cursor by 1 and will wrap around to zero if it will equal
                                // 'buffer_len'.

  size_t buf_tail;              // the position of the read cursor. Reading from the buffer advances
                                // the cursor by 1 and will wrap around to zero if it will equal
                                // 'buffer_len'.
};

#define DEBUG_DEVICE        0xFFFF

typedef struct _STDIO_FILE FILE;
typedef unsigned long long fpos_t;

extern FILE _stdio_open_files[FOPEN_MAX];

#define stdin	(&_stdio_open_files[0])
#define stdout  (&_stdio_open_files[1])
#define stderr  (&_stdio_open_files[2])

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define getc(stream)       fgetc(stream)
#define putc(c, stream)    fputc(c, stream)

void clearerr(FILE *stream);
int fclose(FILE *stream);
int feof(FILE *fp);
int ferror(FILE *fp);
int fflush(FILE *stream);
int fgetc(FILE *stream);
FILE *fopen(char *filename, char *mode);
int fprintf(FILE *stream, const char *format, ...);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int fread(void *ptr, size_t size, size_t count, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int getchar(void);
void perror(const char *prefix);
int printf(const char *format, ...);
int putchar(int c);
int puts(const char *s);
void rewind(FILE *stream);
int sprintf(char *str, const char *format, ...);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
int vprintf(const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STDIO_H */
