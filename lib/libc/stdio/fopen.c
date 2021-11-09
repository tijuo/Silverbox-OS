#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <os/dev_interface.h>
#include <os/vfs.h>
#include <errno.h>

FILE *fopen(char *filename, char *mode)
{
  if(!filename || !mode)
  {
    errno = -EINVAL;
    goto ret_null;
  }

  FILE *file = NULL;
  struct FileAttributes attrib;
  int stat;

  for(size_t i=0; i < FOPEN_MAX; i++)
  {
    if(!_stdio_open_files[i].is_open)
    {
      file = &_stdio_open_files[i];
      break;
    }
  }

  if( !file )
  {
    errno = -EMFILE;
    goto ret_null;
  }

  memset(file, 0, sizeof *file);

  file->filename_len = strlen(filename);

  if( file->filename_len == 0 )
    goto ret_null;

  if( (stat=getFileAttributes(filename, &attrib)) != 0 )
    goto ret_null;

  strncpy(file->filename, filename, file->filename_len);

  file->write_req_buffer = malloc(BUFSIZ + sizeof(struct DeviceOpRequest));

  if( !file->write_req_buffer )
    goto ret_null;

  file->buffer = (char *)&file->write_req_buffer[sizeof(struct DeviceOpRequest)];

  file->file_len = (size_t)attrib.size;
  file->buffer_len = BUFSIZ;
  file->buf_mode = _IOFBF;
  file->orientation = NO_ORI;

  switch( mode[0] )
  {
    case 'w':
      file->access = ACCESS_WR;
      break;
    case 'r':
      file->access = ACCESS_RD;
      break;
    case 'a':
      file->access = ACCESS_WR;
      break;
    default:
      errno = -EINVAL;
      goto free_buffer;
  }

  for(size_t i=1; ; i++)
  {
    switch(mode[i])
    {
      case '\0':
        break;
      case '+':
        file->is_update = 1;
        break;
      case 'b':
        file->is_binary = 1;
        break;
      default:
        break;
    }
  }

  file->is_open = 1;

  return file;

free_buffer:
  free(file->write_req_buffer);

ret_null:
  return NULL;
}
