#include <time.h>
#include <os/syscalls.h>

extern void print(const char *);
extern void printInt(int);

int main(int argc, char *argv[])
{
  time_t t;

  print("\n\n\n\n\n\nAbout to get the time.\n");
  sys_wait(1000);

  time(&t);
  print("The current time is: ");
  print(ctime(&t));
  print("\n");

  return 0;
}
