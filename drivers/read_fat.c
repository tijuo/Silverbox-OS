#include <oslib.h>
#include <os/vfs.h>
#include <string.h>
#include <stdlib.h>
#include <os/services.h>
#include <os/dev_interface.h>
#include <os/console.h>

extern int fatGetDirList( const char *path, unsigned short devNum, struct FileAttributes *attrib, 
                   size_t maxEntries );
extern int fatReadFile( const char *path, unsigned int offset, unsigned short devNum, char *fileBuffer, size_t length );
extern int fatCreateDir( const char *path, const char *name, unsigned short devNum );
extern int fatCreateFile( const char *path, const char *name, unsigned short devNum );

int main(void)
{
  int n;
  mapMem( (void *)0xB8000, (void *)0xB8000, 8, 0 );
//  __map( 0xB8000, 0xB8000, 8 );

  struct FileAttributes *attrib = malloc( 10 * sizeof(struct FileAttributes) );

  __sleep( 2000 );

  printMsg("\n\n\n\n");

/*
  if( fatCreateDir( "/", "system", 0x0A00 ) < 0 )
    printMsg("Couldn't create dir\n");;
*/
  if( fatCreateFile( "/", "test2.txt", 0xA00 ) < 0 )
    printMsg("Couldn't create file\n");

  fatCreateFile( "/", "code.c", 0xA00 );
  fatCreateFile( "/", "stuff", 0xA00 );

  if( (n=fatGetDirList( "/system", 0x0A00, attrib, 10 )) < 0 )
    printMsg("fatGetDirList() failed!\n");
  else
    printMsg("Success\n");


  for( int i=0; i < n; i++ )
  {
    for( int j=0; j < attrib[i].nameLen; j++ )
      printChar( attrib[i].name[j] );
//    deviceWrite( video_srv, 0, 0, attrib[i].nameLen, 1, attrib[i].name );
    printMsg("\n");
  }

  if( (n=fatGetDirList( "/system/..", 0x0A00, attrib, 10 )) < 0 )
    printMsg("fatGetDirList() failed!\n");
  else
    printMsg("Success\n");


  for( int i=0; i < n; i++ )
  {
    for( int j=0; j < attrib[i].nameLen; j++ )
      printChar( attrib[i].name[j] );

//    deviceWrite( video_srv, 0, 0, attrib[i].nameLen, 1, attrib[i].name );
    printMsg("\n");
  }

/*
  printMsg("Reading file...\n");
  
  if( (n=fatReadFile("/regs.txt", 0, 0xA00, attrib, 10 * sizeof( struct FileAttributes ) )) < 0 )
    printMsg("Fail\n");
  else
  {
    printMsg("Successfully read ");
    printMsg(toIntString(n));
    printMsg(" bytes\n\n");
  //  deviceWrite( video_srv, 0, 0, n, 1, attrib );
  }
*/
  while(1);

  return 0;
}
