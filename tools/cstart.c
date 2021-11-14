#include <oslib.h>
#include <os/mutex.h>
#include <os/syscalls.h>
#include <stdlib.h>

extern int main(int, char**);

void _start(void);

void _start(void) {
  int retVal;

  /* Init stuff here */

  retVal = main(0, NULL);

  /* Cleanup stuff here */

  exit(retVal);
}
