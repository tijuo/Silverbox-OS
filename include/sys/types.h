#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <types.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};

#endif /* SYS_TYPES_H */
