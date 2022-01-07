#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/types/bitmap.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

static void *test_setup_bitmap(const MunitParameter params[],
    void *data);

static void test_teardown_bitmap(void *fixture);

static MunitResult test_bitmap_init(const MunitParameter params[],
    void *data);
static MunitResult test_bitmap_set(const MunitParameter params[],
    void *data);
static MunitResult test_bitmap_clear(const MunitParameter params[],
    void *data);
static MunitResult test_bitmap_find_first_set(const MunitParameter params[], void *data);
static MunitResult test_bitmap_find_last_set(const MunitParameter params[], void *data);

#define INIT_PARAM_NAME     "init_with"
#define ZERO_PARAM_VALUE    "0"
#define ONE_PARAM_VALUE     "1"

#define BIT_COUNT           1024

static char *init_params[] = { ZERO_PARAM_VALUE, ONE_PARAM_VALUE, NULL };
static char *zero_params[] = { ZERO_PARAM_VALUE, NULL };
static char *one_params[] = { ONE_PARAM_VALUE, NULL };

static MunitParameterEnum test_init_params[] = {
  { INIT_PARAM_NAME, init_params },
  { NULL, NULL }
};

static MunitParameterEnum test_zero_params[] = {
  { INIT_PARAM_NAME, zero_params },
  { NULL, NULL }
};

static MunitParameterEnum test_one_params[] = {
  { INIT_PARAM_NAME, one_params },
  { NULL, NULL }
};

MunitTest tests[] = {
  {
    "::init()",
    test_bitmap_init,
    test_setup_bitmap,
    test_teardown_bitmap,
    MUNIT_TEST_OPTION_NONE,
    test_init_params
  },
  {
    "::set()",
    test_bitmap_set,
    test_setup_bitmap,
    test_teardown_bitmap,
    MUNIT_TEST_OPTION_NONE,
    test_zero_params
  },
  {
    "::clear()",
    test_bitmap_clear,
    test_setup_bitmap,
    test_teardown_bitmap,
    MUNIT_TEST_OPTION_NONE,
    test_one_params
  },
  {
    "::find_first_set()",
    test_bitmap_find_first_set,
    test_setup_bitmap,
    test_teardown_bitmap,
    MUNIT_TEST_OPTION_NONE,
    test_zero_params
  },
  {
    "::find_last_set()",
    test_bitmap_find_last_set,
    test_setup_bitmap,
    test_teardown_bitmap,
    MUNIT_TEST_OPTION_NONE,
    test_zero_params
  },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
  "kernel/types/bitmap.c",
  tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

void *test_setup_bitmap(const MunitParameter params[],
    void *data) {

  bitmap_t *b = munit_malloc(sizeof *b);
  bool set_bits = false;

  if(b == NULL)
    return NULL;

  for(size_t i=0; params[i].name; i++) {
    if(strcmp(params[i].name, INIT_PARAM_NAME) == 0
      && strcmp(params[i].value, ONE_PARAM_VALUE) == 0) {
      set_bits = true;
      break;
    }
  }

  if(IS_ERROR(bitmap_init(b, BIT_COUNT, set_bits))) {
    free(b);
    return NULL;
  }
  else
    return b;
}

void test_teardown_bitmap(void *fixture) {
  if(fixture) {
    bitmap_release((bitmap_t *)fixture);
    free(fixture);
  }
}

MunitResult test_bitmap_init(const MunitParameter params[],
    void *data) {
  bitmap_t *b = (bitmap_t *)data;
  bool set_bits=false;

  if(b == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; params[i].name; i++) {
    if(strcmp(params[i].name, INIT_PARAM_NAME) == 0
      && strcmp(params[i].value, ONE_PARAM_VALUE) == 0) {
      set_bits = true;
      break;
    }
  }

  assert_int(bitmap_find(b, !set_bits), ==, E_NOT_FOUND);

  for(size_t i=0; i < b->count; i++) {
    if(set_bits)
        assert_true(bitmap_is_set(b, i));
    else
        assert_true(bitmap_is_clear(b, i));
  }

  return MUNIT_OK;
}

MunitResult test_bitmap_set(const MunitParameter params[],
    void *data) {
  bitmap_t *b = (bitmap_t *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  bitmap_set(b, 10);
  bitmap_set(b, 125);
  bitmap_set(b, 993);
  bitmap_set(b, 528);
  bitmap_set(b, 777);

  assert_true(bitmap_is_set(b, 10));
  assert_true(bitmap_is_set(b, 125));
  assert_true(bitmap_is_set(b, 993));
  assert_true(bitmap_is_set(b, 528));
  assert_true(bitmap_is_set(b, 777));

  assert_false(bitmap_is_set(b, 522));
  assert_false(bitmap_is_set(b, 742));
  assert_false(bitmap_is_set(b, 3));
  assert_false(bitmap_is_set(b, 857));
  assert_false(bitmap_is_set(b, 1000));

  return MUNIT_OK;
}

MunitResult test_bitmap_clear(const MunitParameter params[],
    void *data) {
  bitmap_t *b = (bitmap_t *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  bitmap_clear(b, 83);
  bitmap_clear(b, 67);
  bitmap_clear(b, 111);
  bitmap_clear(b, 222);
  bitmap_clear(b, BIT_COUNT-1);

  assert_true(bitmap_is_clear(b, 83));
  assert_true(bitmap_is_clear(b, 67));
  assert_true(bitmap_is_clear(b, 111));
  assert_true(bitmap_is_clear(b, 222));
  assert_true(bitmap_is_clear(b, BIT_COUNT-1));

  assert_false(bitmap_is_clear(b, 123));
  assert_false(bitmap_is_clear(b, 234));
  assert_false(bitmap_is_clear(b, 667));
  assert_false(bitmap_is_clear(b, 900));
  assert_false(bitmap_is_clear(b, 999));
  assert_false(bitmap_is_clear(b, BIT_COUNT));

  return MUNIT_OK;
}

MunitResult test_bitmap_find_first_set(const MunitParameter params[],
    void *data) {
  bitmap_t *b = (bitmap_t *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  bitmap_set(b, 10);
  bitmap_set(b, 125);
  bitmap_set(b, 993);
  bitmap_set(b, 528);
  bitmap_set(b, 777);

  assert_int(bitmap_find_first_set(b), ==, 10);
  bitmap_clear(b, 10);

  assert_int(bitmap_find_first_set(b), ==, 125);
  bitmap_clear(b, 125);

  assert_int(bitmap_find_first_set(b), ==, 528);
  bitmap_clear(b, 528);

  assert_int(bitmap_find_first_set(b), ==, 777);
  bitmap_clear(b, 777);

  assert_int(bitmap_find_first_set(b), ==, 993);
  bitmap_clear(b, 993);

  return MUNIT_OK;
}

MunitResult test_bitmap_find_last_set(const MunitParameter params[],
    void *data) {
  bitmap_t *b = (bitmap_t *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  bitmap_set(b, 10);
  bitmap_set(b, 125);
  bitmap_set(b, 993);
  bitmap_set(b, 528);
  bitmap_set(b, 777);

  assert_int(bitmap_find_last_set(b), ==, 993);
  bitmap_clear(b, 993);

  assert_int(bitmap_find_last_set(b), ==, 777);
  bitmap_clear(b, 777);

  assert_int(bitmap_find_last_set(b), ==, 528);
  bitmap_clear(b, 528);

  assert_int(bitmap_find_last_set(b), ==, 125);
  bitmap_clear(b, 125);

  assert_int(bitmap_find_last_set(b), ==, 10);
  bitmap_clear(b, 10);

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
