#include <oslib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/variables.h>
#include <ctype.h>
#include <os/vfs.h>
#include <os/services.h>
// #include <os/fatfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

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

/* Joins two paths together after truncating '.' and '..' entries */

static int concatPaths( char *_path, const char *_str, char **newPath )
{
  char **stack=NULL;
  char *str, *path;
  unsigned stackPtr=1;
  char *strPtr, *nextPtr=NULL;
  size_t slen, plen;

  if( _path == NULL || newPath == NULL || _str == NULL )
    return -1;

  slen = strlen(_str);
  plen = strlen(_path);

  if( plen == 0 || slen == 0 )
    return -1;

  if( _str[slen-1] != '/' )
    str = strappend(_str, "/");
  else
    str = strdup(_str);

  if( !str )
    return -1;

  if( _path[plen-1] != '/' )
    path = strappend(_path, "/");
  else
    path = strdup(_path);

  if( !path )
  {
    free(str);
    return -1;
  }

  stack = (char **)calloc(512, sizeof(char *));

  if( !stack )
  {
    free(str);
    free(path);
    return -1;
  }

  stack[0] = "";

  strPtr = path;

  while( (nextPtr=strchr( strPtr, '/' )) != NULL && stackPtr < 512 )
  {
    if( nextPtr != strPtr )
    {
      *nextPtr = '\0';

      if( strPtr[1] == '\0' && strncmp( strPtr, ".", 1 ) == 0 )
        ;
      else if( strPtr[2] == '\0' && strncmp( strPtr, "..", 2 ) == 0 && stackPtr > 0 )
        stackPtr--;
      else
        stack[stackPtr++] = strPtr;
    }

    strPtr = nextPtr + 1;
  }

  if( stackPtr < 512 && *strPtr != '\0' )
    stack[stackPtr++] = strPtr;

  strPtr = str;

  while( (nextPtr=strchr( strPtr, '/' )) != NULL && stackPtr < 512 )
  {
    if( nextPtr != strPtr )
    {
      *nextPtr = '\0';

      if( strPtr[1] == '\0' && strncmp( strPtr, ".", 1 ) == 0 )
        ;
      else if( strPtr[2] == '\0' && strncmp( strPtr, "..", 2 ) == 0 && stackPtr > 0 )
        stackPtr--;
      else
        stack[stackPtr++] = strPtr;
    }

    strPtr = nextPtr + 1;
  }

  if( stackPtr < 512 && *strPtr != '\0' )
    stack[stackPtr++] = strPtr;

  for(unsigned i=0; i < stackPtr; i++)
  {
    char *oldPath = *newPath;
    char *addPath = stack[i];

    if( i < stackPtr - 1 || stackPtr == 1 )
      addPath = strappend(stack[i], "/");

    *newPath = strappend(*newPath, addPath);

    if( addPath != stack[i] )
      free(addPath);

    free(oldPath);

    if( !*newPath )
    {
      free(path);
      free(str);
      free(stack);
      return -1;
    }
  }

  free(path);
  free(stack);
  free(str);
  return 0;
}

char *getFullPathName(char *str)
{
  char *path, *dir;

  if( str == NULL )
    return str;

  while( *str == ' ' )
    str++;

  dir = strndup(currDir, PATH_LEN);
  concatPaths( dir, str, &path);

//  printf("Path: '%s'\n", path);

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
    printf("cd [path] - Change current directory\n");
    printf("wd - Display the current working directory\n");
    printf("exec <path> - Explicitly execute a file\n");
    printf("echo <msg> - Prints a message\n");
    printf("time - Displays the current time\n");
    printf("help OR ? - Prints this message\n");
  }
  else if( strncmp( command, "read", 4 ) == 0 )
  {

    int result;
    int len;
    void *file_buffer = NULL;
    char *path = getFullPathName( arg_str );
    FILE *stream = fopen(path, "r");

    if( !stream )
    {
      printf("Unable to open stream\n");
      return -1;
    }

    fseek(stream, 0, SEEK_END);
    len = ftell(stream);

    if( len < 0 )
      return 0;
    else if( len == 0 )
    {
      printf("Empty\n");
      return 0;
    }

    file_buffer = malloc( len );
    fseek(stream, 0, SEEK_SET);

    result = fread(file_buffer, 1, len, stream);

    if( !file_buffer || result < 0 )
      return -1;
    else if( len > 0 )
      printf("%.*s\n", result, file_buffer );

    free(file_buffer);
    fclose(stream);
  }
  else if( strncmp( command, "wd", 2 ) == 0 )
    printf("%.*s\n", PATH_LEN, currDir);
  else if( strncmp( command, "cd", 2 ) == 0 )
  {
    char *path;
    size_t pathLen;

    if( arg_str == NULL || strlen(arg_str) == 0 )
      path = "/";
    else if( arg_str[0] == '/' )
      path = arg_str;
    else
      path = getFullPathName( arg_str );

    if( getFileAttributes( path, attrib_list ) >= 0 )
      strncpy( currDir, path, PATH_LEN );
    else
      return -1;
  }
  else if( strncmp( command, "time", 4 ) == 0 )
  {
    time_t t = time(NULL);

    printf("The time/date is: %s", asctime(gmtime(&t)));
  }
  else if( strncmp( command, "ld", 2 ) == 0 )
  {
    int n;
    char *path, *buffer;
    size_t pathLen;

    if( arg_str == NULL || strlen(arg_str) == 0 )
      path = currDir;
    else
      path = getFullPathName(arg_str);

    n = listDir(path, sizeof attrib_list / sizeof(struct FileAttributes), attrib_list);

    if( n < 0 )
    {
      printf("Failure\n");

      return -1;
    }

    printf("Listing %d entries of %.*s:\n", n, strlen(path), path);

    if( n == 0 )
      printf("No entries\n");
    else
    {
      for(unsigned i=0; i < (unsigned)n; i++)
        printf("%.*s%c\n", attrib_list[i].name_len, attrib_list[i].name, ((attrib_list[i].flags & FS_DIR) ? '/' : '\0'));
    }
  }
  else if( strncmp( command, "echo", 4 ) == 0 )
  {
    if( arg_str )
      printf("%.*s\n", comm_len, arg_str);
  }
  else if( strncmp( command, "exec", 4 ) == 0 )
  {
    char *fName = strtok(arg_str, " "), *args="";
    printf("Executing '%s'\n", fName);
    return exec(fName, args);
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

//  parseDevice(dev_name, &dev_num);

  dev_num = 0xA00;

  charBuf = malloc(BUF_LEN);
  currDir = malloc(PATH_LEN);

  if( charBuf == NULL || currDir == NULL )
    return -1;

  currDir[0] = '/';
  currDir[1] = '\0';

  if( mountFs(dev_num, "fat",3, "/", 35) != 0 )
    print("Mount failed\n");

//  if( unmountFs("/devices/ramdisk0") == 0 )
//    printf("Unmounted\n");

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
int stat;
    if( (stat=doCommand( charBuf, index, second_arg )) != 0 )
      printf("Error: Bad command or argument. %d\n", stat);
  }
  return 1;
}
