#include <oslib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <ctype.h>
#include <os/vfs.h>
#include <os/services.h>
#include <os/fatfs.h>
#include <stdlib.h>
#include <stdio.h>

struct FileAttributes attrib_list[64];

#define BUF_LEN		2048
#define PATH_LEN	4096

char *charBuf = NULL;
char *currDir = NULL;

struct Dev
{
  unsigned char major;
  unsigned char minor;
  size_t name_len;
  char name[11]; // 11 is arbitrarily chosen...
};

short dev_num;
char *dev_name = "rd0";

//char convert_raw_char( unsigned char c );
/*
int printChar( char c )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, 1, 1, &c );
}

int printMsg( char *msg )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, strlen(msg), 1, msg );
}

char getChar( void )
{
  unsigned char kbChar;

  if( kb_srv == NULL_TID )
    kb_srv = lookupName("keyboard", strlen("keyboard"));

  deviceRead( kb_srv, 0, 0, 1, 1, &kbChar );

  return convert_raw_char(kbChar);
}
*/
size_t getStr( char *buffer, size_t maxLen )
{
  int kbChar;
  unsigned num_read;

  for( num_read=0; num_read < maxLen; num_read++ )
  {
    do 
    { 
      kbChar = getchar(); 

      if( kbChar && kbChar != '\b' ) 
        putchar(kbChar); 
      else if( kbChar == '\b' && num_read > 0 )
        printf("\b \b");

      fflush(stdout);

    } while( !kbChar );

    if( kbChar == '\b' )
    {
      num_read--, num_read = --num_read < 0 ? 0 : num_read;
      continue;
    }

    if( (buffer[num_read] = kbChar) == '\n' )
    {
      num_read++;
      break;
    }
  }

  return num_read;
}

static int concatPaths( char *path, char *str )
{
  char *stack[256];
  unsigned stackPtr=0;
  char *strPtr = str, *nextPtr;

  if( path == NULL || str == NULL )
    return -1;

  while( (nextPtr=strchr( strPtr, '/' )) != NULL && stackPtr < 256 )
  {
    if( nextPtr != strPtr )
    {
      *nextPtr = '\0';

      if( strncmp( strPtr, ".", 1 ) == 0 )
        ;
      else if( strncmp( strPtr, "..", 2 ) == 0 && stackPtr > 0 )
        stackPtr--;
      else
        stack[stackPtr++] = strPtr;
    }

    strPtr = nextPtr + 1;
  }

  if( stackPtr < 256 )
    stack[stackPtr++] = strPtr;

      // FIXME: The following may cause a buffer overflow if the argument is too long

  for(unsigned i=0; i < stackPtr; i++)
  {
    size_t len = strlen(stack[i]);
    stack[i][len] = '/';

    strncat(path, stack[i], len+1);
  }

  return 0;
}

char *getFullPathName(char *str)
{
  static char path[PATH_LEN+1];
  
  if( str == NULL )
    return str;

  while( *str == ' ' )
    str++;

  if( str[0] == '/' )
    return str;

  path[PATH_LEN] = '\0';
  strncpy( path, currDir, PATH_LEN );
  concatPaths( path, str );
//  strncpy( &path[len], str, PATH_LEN );

  return path;
}

int parseDevice( const char *str, short *devNum )
{
  unsigned minorIndex=1;
  char major, minor;
  struct Device device;
  long _minor;

  if( !isalpha(str[0]) )
    return -1;

  while( isalpha(str[minorIndex]) )
    minorIndex++;

  if( lookupDevName(str, minorIndex, &device) != 0 )
    return -1;

  major = device.major;

  _minor = strtol(&str[minorIndex], NULL, 10);

  if( _minor < 0 || minor > 255 )
    return -1;

  minor = _minor;

  if( devNum )
    *devNum = (major << 8) | minor;

  return 0;
}
/*
int parsePath( const char *path, short *devNum, char **pathStart )
{
  unsigned minorIndex=1;
  char major;

  if( !isalpha(path[0]) )
    return -1;

  while( isalpha(path[minorIndex]) )
    minorIndex++;

  if( !isdigit(path[minorIndex]) )
    return -1;

  if( memcmp( &path[minorIndex+1], ":/", 2 ) != 0 )
    return -1;

  major = lookupName(path, minorIndex);

  if( pathStart )
    *pathStart = &path[minorIndex+3];

  if( devNum )
    *devNum = (major << 8) | (path[minorIndex] - '0');

  return 0;
}
*/

int doCommand( char *command, size_t comm_len, char *arg_str )
{
  if( strncmp( command, "help", 4 ) == 0 || strncmp( command, "?", 1 ) == 0 )
  {
    printf("read <path> - Read the contents of a file\n");
    printf("ld [path] - List the contents of a directory\n");
    printf("cd <path> - Change current directory\n");
    printf("wd - Display the current working directory\n");
    printf("echo <msg> - Prints a message\n");
    printf("help OR ? - Prints this message\n");
  }
  else if( strncmp( command, "read", 4 ) == 0 )
  {
    int result;
    char *path = getFullPathName( arg_str );

    char *file_buffer = malloc( 1920 );

    result = fatReadFile( path, 0, dev_num,
                 file_buffer, 1920 );
    free(file_buffer);

    if( result < 0 )
      printf("Failed to read %s\n", path);
    else
      printf("%.*s\n", result, file_buffer );
  }
  else if( strncmp( command, "wd", 2 ) == 0 )
    printf("%.*s\n", PATH_LEN, currDir);
  else if( strncmp( command, "cd", 2 ) == 0 )
  {
    char *path;

    if( comm_len == 0 || arg_str == NULL )
      path = "/";
    else
      path = getFullPathName( arg_str );

    if( fatGetAttributes( path, dev_num, attrib_list ) >= 0 )
      strncpy( currDir, path, PATH_LEN );
    else
      return -1;
  }
  else if( strncmp( command, "ld", 2 ) == 0 )
  {
    int n;
    char *path, *buffer;

    if( comm_len == 0 || arg_str == NULL )
      path = currDir;
    else
      path = getFullPathName(arg_str );

    n = fatGetDirList(path, dev_num, attrib_list, sizeof attrib_list / 
                      sizeof(struct FileAttributes));

    if( n < 0 )
    {
      printf("Failure\n");

      return -1;
    }

    printf("Listing of %.*s:\n", PATH_LEN, arg_str);

    if( n == 0 )
      printf("No entries\n");
    else
    {
      for(unsigned i=0; i < (unsigned)n; i++)
        printf("%.*s%c\n", attrib_list[i].nameLen, attrib_list[i].name, ((attrib_list[i].flags & FS_DIR) ? '/' : '\0'));
    }
  }
  else if( strncmp( command, "echo", 4 ) == 0 )
  {
    if( arg_str )
      printf("%.*s\n", comm_len, arg_str);
  }
  else
  {
    printf("Unknown command!\n");
    return -1;
  }
  return 0;
}

int main(void)
{
  char *space_ptr, *second_arg;
  size_t index;

//  mapMem((void *)0xB8000, (void *)0xB8000, 8, 0);
//  dev_num = lookupName( dev_name, strlen(dev_name) );

  parseDevice(dev_name, &dev_num);

  dev_num = 0xA00;

  charBuf = malloc(BUF_LEN);
  currDir = malloc(PATH_LEN);

  if( charBuf == NULL || currDir == NULL )
    return -1;

  currDir[0] = '/';
  currDir[1] = '\0';

  while( true )
  {
    printf("%s:%.*s>", dev_name, PATH_LEN, currDir);
    fflush(stdout);

    index = getStr(charBuf, BUF_LEN);

    charBuf[index] = 0;

    if( (space_ptr = strpbrk( charBuf, " \n" )) == NULL )
    {
      printf("\nCommand is too long - ignoring...\n");
      continue;
    }

    if( index == 0 )
    {
      printf("Strange error (index is 0?!)...\n");
      continue;
    }

    charBuf[index-1] = '\0';

    second_arg = *space_ptr ? space_ptr + 1 : NULL;

    if( doCommand( charBuf, index, second_arg ) < 0 )
      printf("Error: Bad command or argument.'%.*s'\n", index, charBuf);
  }
  return 1;
}
