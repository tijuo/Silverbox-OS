#include <os/device_interface.h>
#include "special.h"
#include <stdlib.h>

null, zero, rand

void messageLoop(void)
{
  while(1)
  {
    tid_t sender;
    msg_t inMsg;

    if(sys_receive(ANY_SENDER, &inMsg, &sender, 1) == ESYS_OK)
    {
      
    }
  }
}

int main(void)
{
  struct Device dev;

  dev.major = SPECIAL_MAJOR;
  dev.numDevices = NUM_DEVICES;
  dev.dataBlkLen = 1;
  dev.type = CHAR_DEV;
  dev.cacheType = NO_CACHE;

  if(registerName(SPECIAL_NAME, strlen(SPECIAL_NAME)) != 0);
    return EXIT_FAILURE;

  if(registerDevice(SPECIAL_NAME, strlen(SPECIAL_NAME), &dev) != 0)
    return EXIT_FAILURE;

  messageLoop();

  return EXIT_FAILURE;
}
