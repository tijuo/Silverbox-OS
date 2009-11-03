#include <oslib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/vfs.h>
#include <os/services.h>
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

unsigned short dev_num = 0xA00;

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

/*
char convert_raw_char( unsigned char c )
{
  static int alt=0, caps=0, shift=0, numlk=0;

  char num_shift[10] = { ')', '!', '@', '#', '$', '%', '^', '&', '*','(' };

  if( (c & VK_BREAK) == VK_BREAK )
  {
    if( (c & 0x7F) == VK_RSHIFT || (c & 0x7F) == VK_LSHIFT )
      shift = 0;
    return '\0';
  }
  else if( c == VK_CAPSLK )
    caps ^= 1;
  else if( c == VK_NUMLK )
    numlk ^= 1;

  c &= 0x7F;

  if( c >= VK_0 && c <= VK_9 )
    return (shift ? num_shift[c-VK_0] : '0' + (c - VK_0));
  else if( c >= VK_A && c <= VK_Z )
    return ((!shift && !caps) || (shift && caps) ? 'a' + (c - VK_A) : 'A' + (c-VK_A));
  else if( c == VK_RSHIFT || c == VK_LSHIFT )
    shift = 1;

  switch( c )
  {
    case VK_ENTER:
      return '\n';
    case VK_BSPACE:
      return '\b';
    case VK_TAB:
      return '\t';
    case VK_SPACE:
      return ' ';
    case VK_MINUS:
      return (shift ? '_' : '-');
    case VK_EQUAL:
      return (shift ? '+' : '=');
    case VK_BQUOTE:
      return (shift ? '~' : '`');
    case VK_COMMA:
      return (shift ? '<' : ',');
    case VK_PERIOD:
      return (shift ? '>' : '.');
    case VK_SLASH:
      return (shift ? '?' : '/');
    case VK_BSLASH:
      return (shift ? '|' : '\\');
    case VK_RBRACK:
      return (shift ? '}' : ']');
    case VK_LBRACK:
      return (shift ? '{' : '[');
    case VK_QUOTE:
      return (shift ? '"' : '\'');
    case VK_SEMICOL:
      return (shift ? ':' : ';');
    default:
      return '\0';
  }
  return '\0';
}
*/
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
/*
    char *file_buffer;

    

     = malloc( (size_t)
//    fatReadFile
*/

    printf("read isn't supported yet...\n");
  }
  else if( strncmp( command, "wd", 2 ) == 0 )
    printf("%.*s\n", PATH_LEN, currDir);
  else if( strncmp( command, "cd", 2 ) == 0 )
  {
    if( arg_str[0] == '/' )
    {
      if( fatGetAttributes(arg_str, dev_num, attrib_list ) >= 0 )
        strncpy( currDir, arg_str, PATH_LEN );
      else
        return -1;
    }
    else
      concatPaths( currDir, arg_str );
  }
  else if( strncmp( command, "ld", 2 ) == 0 )
  {
    int n;
    char *path=arg_str, *buffer;

    if( buffer == NULL )
      return -1;

    if( arg_str == NULL )
      path = currDir;
    else if( arg_str[0] != '/' )
    {
      buffer = malloc(PATH_LEN);

      if( buffer == NULL )
        return -1;

      memcpy( buffer, currDir, PATH_LEN );
      concatPaths( buffer, arg_str );
      path = buffer;
    }

    n = fatGetDirList(path, dev_num, attrib_list, sizeof attrib_list / 
                      sizeof(struct FileAttributes));

    if( n < 0 )
    {
      printf("n < 0\n");

      if( arg_str != NULL && *arg_str != '/' )
        free(buffer);

      return -1;
    }

    printf("Listing of %.*s:\n", PATH_LEN, path);

    if( n == 0 )
      printf("No entries\n");
    else
    {
      for(unsigned i=0; i < n; i++)
      {
        for( unsigned j=0; j < attrib_list[i].nameLen; j++ )
          putchar(attrib_list[i].name[j]);
        
        if( attrib_list[i].flags & FS_DIR )
          putchar('/');

        putchar('\n');
      }
    }

    if( arg_str != NULL && *arg_str != '/' )
      free(buffer);
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

  __map(0xb8000, 0xb8000, 8);

  charBuf = malloc(BUF_LEN);
  currDir = malloc(PATH_LEN);

  if( charBuf == NULL || currDir == NULL )
    return -1;

  currDir[0] = '/';
  currDir[1] = '\0';

  while( true )
  {
    printf("rd0:/> ");
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
