#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <os/syscalls.h>
#include <os/msg/rtc.h>
#include <oslib.h>
#include <os/services.h>

int main(void)
{
  time_t t;

  fprintf(stderr, "Waiting for the time server...\n");

  while(lookupName(RTC_NAME) == NULL_TID)
    sys_wait(500);

  time(&t);
  fprintf(stderr, "The current time is: %s\n", ctime(&t));

  return EXIT_SUCCESS;
}
