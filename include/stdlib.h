#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

#define EXIT_SUCCESS	0
#define	EXIT_FAILURE	1

#define RAND_MAX	0xFFFFFFFFUL

#ifdef __cplusplus
extern "C" {
#endif

int abs(int num);
long labs(long n);

div_t div(int num, int denom);
ldiv_t ldiv(long num, long denom);
lldiv_t lldiv(long long num, long long denom);

int atoi(char *nptr);
long atol(char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

void abort(void);
void exit(int status);
int atexit(void (*func)(void));

void *malloc(size_t bytes);
void free(void *address);
void *calloc( size_t n_elements, size_t element_size );
void *realloc(void *p, size_t n);

/* Extra stuff */

void *memalign(size_t alignment, size_t n);
void *valloc(size_t n);

/* Extra dlmalloc stuff */

int mallopt(int parameter_number, int parameter_value);
size_t malloc_footprint(void);
size_t malloc_max_footprint(void);
struct mallinfo mallinfo(void);
void **independent_calloc(size_t n_elements, size_t element_size,\
                          void *chunks[]);
void **independent_comalloc(size_t n_elements, size_t sizes[], void *chunks[]);
void *pvalloc(size_t n);
int malloc_trim(size_t pad);
size_t malloc_usable_size(void *p);
void malloc_stats(void);
size_t malloc_footprint_limit(void);

void *bsearch(const void *key, const void *base, size_t n, size_t size,
int (*cmp)(const void *, const void *));
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *,
const void *));

int rand(void);
void srand(unsigned int seed);

extern unsigned int __mt_array[624];
extern int __mt_index;

#ifdef __cplusplus
};
#endif

#endif /* STDLIB_H */
