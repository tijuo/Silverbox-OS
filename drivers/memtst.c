#include <stdlib.h>
#include <oslib.h>

int main(void)
{
  unsigned char *p;

  __sleep(5000);

  p = malloc(20000);

  return 0;
}
