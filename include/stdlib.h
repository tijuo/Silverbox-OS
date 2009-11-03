#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;

#define EXIT_SUCCESS	0
#define	EXIT_FAILURE	1

int abs(int num);
long labs(long n);

div_t div(int num, int denom);
ldiv_t ldiv(long num, long denom);

int atoi(char *nptr);
long atol(char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

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

void *bsearch(const void *key, const void *base, size_t n, size_t size, 
int (*cmp)(const void *, const void *));
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, 
const void *));

int rand(void);
void srand(unsigned int seed);

unsigned int __mt_array[624];
int __mt_index;

#ifdef __cplusplus
}
#endif

#endif /* STDLIB_H */
