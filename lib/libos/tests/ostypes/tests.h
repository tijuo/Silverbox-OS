#ifndef SBOS_TESTS
#define SBOS_TESTS

#include <stdio.h>

#define ASSERT(x) do { if(!(x)) { fprintf(stderr, "Assertion %s failed in %s:%d.\n", #x, __FILE__, __LINE__); return -1; } } while(0)
#define ASSERT_TRUE(x)      ASSERT(x)
#define ASSERT_FALSE(x)     ASSERT(!(x))
#define ASSERT_EQ(x, y)     ASSERT((x) == (y))
#define ASSERT_NEQ(x, y)    ASSERT((x) != (y))
#define ASSERT_NULL(x)      ASSERT((x) == NULL)
#define ASSERT_NON_NULL(x)  ASSERT((x) != NULL)
#define ASSERT_NOT_NULL(x)  ASSERT_NON_NULL(x)
#define ASSERT_ZERO(x)      ASSERT((x) == 0)
#define ASSERT_NON_ZERO(x)  ASSERT((x) != 0)

#define TEST(n) (Test) { .name = # n, .test = n, .result = 0 }
#define END_TESTS (Test) { .name = NULL, .test = NULL, .result = 0 }

// XXX: This will not perform clean up on an assertion failure
typedef struct {
    char *name;
    int (*test)(void);
    int result;
} Test;

int tests_run(int argc, char* argv[], Test tests[]);

#endif /* SBOS_TESTS */