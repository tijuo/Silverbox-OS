#include <os/console.h>
#include <os/dev_interface.h>
#include <oslib.h>
#include <string.h>

unsigned char buffer[4096];
unsigned char buffer2[4096];

tid_t rdisk_srv = NULL_TID;

int main(void)
{
  printMsg("Sleeping...");
  __sleep(3500);

  printMsg("Writing buffer\n");
  for(int i=0; i < 4096; i++)
    buffer[i] = ((i * 2) + 3) % 256; 

  printMsg("Looking up ramdisk...\n");

  if( rdisk_srv == NULL_TID )
    rdisk_srv = lookupName("ramdisk", strlen("ramdisk"));

  printMsg("writing...\n");

  deviceWrite( rdisk_srv, 0, 0, 4096 / 512, 512, buffer );
  printMsg("reading...\n");
  deviceRead( rdisk_srv, 0, 0, 4096 / 512, 512, buffer2 );

  for(int i=0; i < 4096; i++)
  {
    if( buffer2[i] != ((i * 2) + 3) % 256 )
    {
      printMsg(toIntString(buffer2[i]));
      printMsg("! ");
      printMsg(toIntString(((i * 2) + 3) % 256));
      printMsg(" ");
    }
  }

  printMsg("done\n");

  __pause();
  return 0;
}
