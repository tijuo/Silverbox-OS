#ifndef KERNEL_LIST_STRUCT_H
#define KERNEL_LIST_STRUCT_H

#include <types.h>

struct List {
  tid_t headTid;
  tid_t tailTid;
};

typedef struct List list_t;

#endif /* KERNEL_LIST_STRUCT_H */
