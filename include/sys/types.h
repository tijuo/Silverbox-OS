#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <types.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

typedef s8    int8_t;
typedef int16 int16_t;
typedef int32 int32_t;
typedef int64 int64_t;

typedef u8    u_int8_t;
typedef uint16 u_int16_t;
typedef uint32 u_int32_t;
typedef uint64 u_int64_t;

typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;

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
