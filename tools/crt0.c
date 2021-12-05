#include <stdlib.h>

extern int main(int, char**);

void _start(void);

void _start(void) {
  int result;
  /* Init stuff here */

  result = main(0, NULL);

  /* Cleanup stuff here */

  exit(result);

}
