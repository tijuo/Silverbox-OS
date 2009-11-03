#ifndef KVARIABLES
#define KVARIABLES

#include <kernel/mm.h>

#define VAR_CLK_SEC	KERNEL_VAR_PAGE
#define VAR_CLK_USEC	(VAR_CLK_SEC + sizeof(unsigned int))
#define VAR_BOOT_TYPE	(VAR_CLK_USEC + sizeof(unsigned int))
#define VAR_PMEM_SIZE	(VAR_BOOT_TYPE + sizeof(unsigned int))
#endif /* KVARIABLES */
