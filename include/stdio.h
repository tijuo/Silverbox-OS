#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF			((int)-1)
#define BUFSIZ			1024
#define FILENAME_MAX		256
#define FOPEN_MAX		16
#define _IOFBF			1
#define _IOLBF			2
#define _IONBF			3
#define L_tmpnam

#ifndef NULL
  #define NULL 0
#endif /* NULL */

#define SEEK_CUR	0
#define SEEK_END	1
#define SEEK_SET	2
#define TMP_MAX

#define ACCESS_RD		1
#define ACCESS_WR		2
#define ACCESS_RD_WR		3

struct _STDIO_FILE
{
  unsigned is_binary : 1;
  unsigned access    : 2;
  unsigned buf_mode  : 2;
  unsigned eof       : 1;
  unsigned error     : 1;
  unsigned user_buf  : 1;
  size_t file_pos;
  size_t file_len;
  short dev_num;
  char *buffer;
  size_t buffer_len; // the size of the buffer
  size_t buf_pos; // 
};

typedef struct _STDIO_FILE FILE;
typedef unsigned long long fpos_t;

extern FILE _std_files[3];

#define stdin	(&_std_files[0])
#define stdout  (&_std_files[1])
#define stderr   (&_std_files[2])

void clearerr(FILE *stream);
int feof(FILE *fp);
int ferror(FILE *fp);
int fflush(FILE *stream);
int fgetc(FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *gets(char *s);
void perror(const char *prefix);
int printf(const char *format, ...);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
void rewind(FILE *stream);
int sprintf(char *str, const char *format, ...);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
int vprintf(const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);

#endif /* STDIO_H */
