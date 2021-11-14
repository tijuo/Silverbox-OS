#include <stdlib.h>

void abort() {
  exit(-1);
}

void exit(int status) {
  __asm__  __volatile__("hlt\n" :: "a"(status));
}
