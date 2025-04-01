#include <sys/time.h>
#include <errno.h>

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    errno = ENOTSUP;
    return -1;
}
