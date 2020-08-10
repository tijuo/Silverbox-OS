#ifndef STDDEF_H
#define STDDEF_H

#ifndef NULL
  #define NULL	   0
#endif

typedef	signed int  	ptrdiff_t;
typedef unsigned int	size_t;

#define offsetof(type,member)		(size_t)&(((type *)0)->member)

#ifndef __cplusplus
typedef short int wchar_t;
#endif /* __cplusplus */

#endif
