#ifndef ASSERT_H
#define ASSERT_H

#include <stdio.h>

#ifdef NDEBUG
#define assert(ignore)((void) 0)
#else
#define assert(x)	{ __typeof__ (x) _x=(x); \
                          if( (_x) == 0 ) fprintf(stderr, "%s: '%s' - Assertion failed on line %d in file '%s'\n", __func__, #x, __LINE__, __FILE__); }
#endif /* NDEBUG */

#define static_assert	_Static_assert

#endif /* ASSERT_H */
