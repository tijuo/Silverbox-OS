#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <sys/types.h>

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

clock_t times(struct tms *);

#endif /* SYS_TYPES_H */
