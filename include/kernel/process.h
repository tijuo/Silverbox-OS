#ifndef PROCESS_H
#define PROCESS_H

#include <kernel/types/vector.h>
#include <types.h>

typedef struct process_control_block {
  paddr_t root_pmap;
  vector_t threads;
} pcb_t;

#endif /* PROCESS_H */
