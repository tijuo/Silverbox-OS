#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/algo.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

static void *test_setup_algo(const MunitParameter params[],
    void *data);
static void test_teardown_algo(void *fixture);

static MunitResult test_algo_kbsearch(const MunitParameter params[],
    void *data);
static MunitResult test_algo_kqsort(const MunitParameter params[],
    void *data);
static int compare(const void *v1, const void *v2);

MunitTest tests[] = {
  {
    "::kbsearch()",
    test_algo_kbsearch,
    test_setup_algo,
    test_teardown_algo,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::kqsort()",
    test_algo_kqsort,
    test_setup_algo,
    test_teardown_algo,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
  "kernel/algo.c",
  tests,
  NULL,
  10,
  MUNIT_SUITE_OPTION_NONE
};

struct array {
  long int *data;
  size_t size;
};

void *test_setup_algo(const MunitParameter params[],
    void *data) {
  struct array *arr = (struct array *)malloc(sizeof *arr);

  if(!arr)
    return NULL;

  size_t bytes = 8192*sizeof(long int);

  if(bytes % sizeof(long int) != 0)
    bytes += sizeof(long int) - (bytes % sizeof(long int));

  munit_uint8_t *buffer = (munit_uint8_t *)malloc(bytes);

  if(!buffer) {
    free(arr);
    return NULL;
  }

  munit_rand_memory(bytes, buffer);

  arr->data = (long int *)buffer;
  arr->size = bytes / sizeof(long int);

  return arr;
}

void test_teardown_algo(void *fixture) {
  if(fixture) {
    struct array *arr = (struct array *)fixture;

    free(arr->data);
    free(arr);
  }
}

static int compare(const void *v1, const void *v2) {
  const long int *x1 = (const long int *)v1;
  const long int *x2 = (const long int *)v2;

  if(*x1 == *x2)
    return 0;
  else if(*x1 > *x2)
    return 1;
  else
    return -1;
}

static MunitResult test_algo_kbsearch(const MunitParameter params[],
    void *data) {
  if(data == NULL)
    return MUNIT_ERROR;

  struct array *arr = (struct array *)data;

  qsort(arr->data, arr->size, sizeof(long int), compare);

  size_t pick = (size_t)munit_rand_int_range(0, arr->size-1);
  long int key = arr->data[pick];
  void *found = kbsearch(&key, arr->data, arr->size, sizeof(long int), compare);

  assert_true(found != NULL);
  assert_long(key, ==, *(long int *)found);

  return MUNIT_OK;
}

static MunitResult test_algo_kqsort(const MunitParameter params[],
    void *data) {
  if(data == NULL)
    return MUNIT_ERROR;

  struct array *arr = (struct array *)data;

  kqsort(arr->data, arr->size, sizeof(long int), compare);

  for(size_t i=1; i < arr->size; i++)
    assert_long(arr->data[i-1], <=, arr->data[i]);

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
