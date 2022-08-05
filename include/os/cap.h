#ifndef OS_CAP_H
#define OS_CAP_H

#define CAP_NULL_INDEX      0

/// Indicates that a right may be transferred to other threads.
#define CAP_RIGHT_GRANT      0x01

/// Indicates that a right applies to all resources of a type.
#define CAP_RIGHT_UNIVERSAL  0x02

#define CAP_RIGHT_READ       0x04
#define CAP_RIGHT_WRITE      0x08
#define CAP_RIGHT_CREATE     0x10
#define CAP_RIGHT_DESTROY    0x20

// Threads
#define CAP_RES_THREAD    0

// Memory map/unmap
#define CAP_RES_MEM       1

// IRQ handling
#define CAP_RES_IRQ       2

// Exception handling
#define CAP_RES_EXCEPT    3

#endif /* OS_CAP_H */
