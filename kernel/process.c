#include <kernel/process.h>
#include <kernel/mm.h>

vector_t process_vector;
struct kmem_cache pcb_mem_cache;
