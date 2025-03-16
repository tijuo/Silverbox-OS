#ifndef KERNEL_LIST_STRUCT_H
#define KERNEL_LIST_STRUCT_H

#include <types.h>

struct List {
    tid_t head_tid;
    tid_t tail_tid;
};

typedef struct List list_t;

#endif /* KERNEL_LIST_STRUCT_H */