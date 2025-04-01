extern int main(int, char**);
extern void exit(int);

void _start(void);

void _start(void) {
  int retVal;

  /* Init stuff here */

  retVal = main(0, 0);

  /* Cleanup stuff here */

  exit(retVal);
}
