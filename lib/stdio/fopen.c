#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include <os/vfs.h>

FILE *fopen(char *filename, char *mode)
{
  FILE *file = calloc(1, sizeof(FILE));
  struct FileAttributes attrib;
  int bytes;

  if( !file )
    return NULL;

  if( !filename || !mode )
  {
    free(file);
    return NULL;
  }

  file->filename_len = strlen(filename);

  if( file->filename_len == 0 )
  {
    free(file);
    return NULL;
  }

  int stat;

  if( (stat=getFileAttributes(filename, &attrib)) != 0 )
  {
    free(file);
    return NULL;
  }

  strncpy(file->filename, filename, file->filename_len);

  switch( mode[0] )
  {
    case 'w':
      file->access = ACCESS_WR;
      break;
    case 'r':
      file->access = ACCESS_RD;
      break;
    case 'a':
    default:
      break;
  }

  if( mode[0] && mode[1] == 'b' )
    file->is_binary = 1;

  file->buffer = malloc(BUFSIZ);

  if( !file->buffer )
  {
    free(file);
    return NULL;
  }

/*  bytes = readFile( filename, 0, file->buffer, BUFSIZ );

  if( bytes < 0 )
  {
    free(file->buffer);
    free(file);
    return NULL;
  }
*/

  file->file_len = (size_t)attrib.size;
  file->buffer_len = BUFSIZ;
  file->buf_mode = _IOFBF;

  return file;
}
