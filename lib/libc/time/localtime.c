#include <time.h>

struct tm* localtime(const time_t *time) {
  return gmtime(time);
}
