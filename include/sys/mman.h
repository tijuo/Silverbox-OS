#ifndef SYS_MMAP_H
#define SYS_MMAP_H

#include <sys/types.h>

#ifndef __off_t_defined
# ifndef __USE_FILE_OFFSET64
typedef __off_t off_t;
# else
typedef __off64_t off_t;
# endif
# define __off_t_defined
#endif

extern void *mmap (void *__addr, size_t __len, int __prot,
		   int __flags, int __fd, __off_t __offset);

#endif /* SYS_MMAP_H */
