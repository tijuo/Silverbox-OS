#include <string.h>

void *memset(void *buffer, int c, size_t num)
{
  register size_t i = num;
  char *buff = (char *)buffer;

  while( i )
  {
    *buff++ = (char)c;
    i--;
  }

  return (void *)buffer;
}

/*
void *memset(void *buffer, int c, size_t num)
{
  unsigned long data = (char)c | ((char)c << 8) | ((char)c << 16) | ((char)c << 24);
  unsigned char *buff = (unsigned char *)buffer;
  unsigned long *buff_s = (unsigned long *)(((unsigned)buffer % 4 == 0) ? (unsigned)buffer : ((unsigned)buffer + (4-(unsigned)buffer % 4)));
  unsigned char *buff_e = (unsigned char *)(((unsigned)buffer + num) - (((unsigned)buffer + num) % 4));

  if( (unsigned)buff_s < (unsigned)buff_e )
  {
    while((unsigned)buff < (unsigned)buff_s)
      *buff++ = (unsigned char)c;
  }

  buff = (unsigned char *)((unsigned)buffer + num);

  while((unsigned)buff_s < (unsigned)buff_e)
    *buff_s++ = data;

  while( (unsigned)buff_e < (unsigned)buff )
    *buff_e++ = (unsigned char)c;

  return buffer;
}
*/
