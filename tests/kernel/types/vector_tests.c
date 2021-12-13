#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/types/vector.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

static void *test_setup_vector(const MunitParameter params[],
    void *data);
static void test_teardown_vector(void *fixture);
static void *test_setup_char_vector(const MunitParameter params[],
    void *data);

static MunitResult test_vector_init(const MunitParameter params[],
    void *data);
static MunitResult test_vector_push_back(const MunitParameter params[],
    void *data);
static MunitResult test_vector_push_front(const MunitParameter params[],
    void *data);
static MunitResult test_vector_getters(const MunitParameter params[], void *data);
static MunitResult test_vector_get_copy(const MunitParameter params[], void *data);
static MunitResult test_vector_index_of(const MunitParameter params[], void *data);
static MunitResult test_vector_rindex_of(const MunitParameter params[], void *data);
static MunitResult test_vector_pop_back(const MunitParameter params[], void *data);
static MunitResult test_vector_pop_front(const MunitParameter params[], void *data);
static MunitResult test_vector_insert(const MunitParameter params[], void *data);
static MunitResult test_vector_remove_item(const MunitParameter params[], void *data);
static MunitResult test_vector_rremove_item(const MunitParameter params[], void *data);
static MunitResult test_vector_remove(const MunitParameter params[], void *data);

MunitTest tests[] = {
  {
    "/init",
    test_vector_init,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/push_back",
    test_vector_push_back,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/push_front",
    test_vector_push_front,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/getters",
    test_vector_getters,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/get_copy",
    test_vector_get_copy,
    test_setup_char_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/index_of",
    test_vector_index_of,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/rindex_of",
    test_vector_rindex_of,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/pop_back",
    test_vector_pop_back,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/pop_front",
    test_vector_pop_front,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/remove_item",
    test_vector_remove_item,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/rremove_item",
    test_vector_rremove_item,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/insert",
    test_vector_insert,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "/remove",
    test_vector_remove,
    test_setup_vector,
    test_teardown_vector,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
  "/vector",
  tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

void *test_setup_vector(const MunitParameter params[],
    void *data) {
  vector_t *v = (vector_t *)malloc(sizeof(vector_t));

  return v != NULL && vector_init(v, sizeof(int)) == E_OK ? v : NULL;
}

void *test_setup_char_vector(const MunitParameter params[],
    void *data) {
  vector_t *v = (vector_t *)malloc(sizeof(vector_t));

  return v != NULL && vector_init(v, sizeof(char)) == E_OK ? v : NULL;
}

void test_teardown_vector(void *fixture) {
  if(fixture) {
    vector_release((vector_t *)fixture);
    free(fixture);
  }
}

MunitResult test_vector_init(const MunitParameter params[],
    void *data) {
  vector_t *v = (vector_t *)data;

  if(v == NULL)
    return MUNIT_ERROR;

  assert_size(vector_get_count(v), ==, 0);
  assert_size(vector_get_capacity(v), >, 0);
  assert_size(vector_get_item_size(v), ==, sizeof(int));

  return MUNIT_OK;
}

MunitResult test_vector_push_back(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[16] = { 0xABCDEF, 23, -44, 17, 9, -384, 10003, 0, -7683295,
                       88888888, -9238400, -23, 0xC0FFEE, 0x1BADDAD, 9, 16 };
  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 16; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_FAIL;
  }

  assert_size(vector_get_count(v), ==, 16);
  assert_size(vector_get_capacity(v), >=, 16);

  for(size_t i=0; i < 16; i++) {
    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data[i]);
  }

  return MUNIT_OK;
}

MunitResult test_vector_push_front(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[16] = { 0xABCDEF, 23, -44, 17, 9, -384, 10003, 0, -7683295,
                       88888888, -9238400, -23, 0xC0FFEE, 0x1BADDAD, 9, 16 };
  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 16; i++) {
    if(vector_push_front(v, &int_data[i]) != E_OK)
      return MUNIT_FAIL;
  }

  assert_size(vector_get_count(v), ==, 16);
  assert_size(vector_get_capacity(v), >=, 16);

  for(size_t i=0; i < 16; i++) {
    assert_int(*(int *)VECTOR_ITEM(v, 16-i-1), ==, int_data[i]);
  }

  return MUNIT_OK;
}

MunitResult test_vector_getters(const MunitParameter params[], void *data) {
  int int_data[8] = { 7, 6, 6, 4, 9, 2, 1 };
  vector_t v = {
    .data = (void *)int_data,
    .item_size = sizeof(int),
    .capacity = 8,
    .count = 4
  };

  assert_size(vector_get_count(&v), ==, 4);
  assert_size(vector_get_capacity(&v), ==, 8);
  assert_size(vector_get_item_size(&v), ==, sizeof(int));

  assert_int(*(int *)VECTOR_ITEM(&v, 2), ==, 6);
  assert_int(*(int *)VECTOR_ITEM(&v, 0), ==, 7);

  return MUNIT_OK;
}

MunitResult test_vector_get_copy(const MunitParameter params[], void *data) {
  static const char *lorem_str = "Lorem ipsum dolor sit amet...";
  size_t lorem_len = strlen(lorem_str);
  char c = '\0';

  vector_t *v = (vector_t *)data;

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < lorem_len; i++) {
    if(IS_ERROR(vector_push_back(v, &lorem_str[i])))
      return MUNIT_ERROR;
  }

  if(IS_ERROR(vector_get_copy(v, 5, &c)))
    return MUNIT_FAIL;

  assert_char(c, ==, ' ');

  if(IS_ERROR(vector_get_copy(v, 12, &c)))
    return MUNIT_FAIL;

  assert_char(c, ==, 'd');

  if(IS_ERROR(vector_get_copy(v, 15, NULL)))
    return MUNIT_FAIL;

  if(!IS_ERROR(vector_get_copy(v, (size_t)0xFFFFFFFFFFFFFul, &c)))
    return MUNIT_FAIL;

  return MUNIT_OK;
}

MunitResult test_vector_index_of(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int palindrome[9] = { 1, 2, 3, 4, 5, 4, 3, 2, 1 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 9; i++) {
    if(vector_push_front(v, &palindrome[i]) != E_OK)
      return MUNIT_ERROR;
  }

  int num = 3;
  int index = vector_index_of(v, &num);

  assert_int(index, !=, E_NOT_FOUND);
  assert_size((size_t)index, ==, 2);

  num = 1;
  index = vector_index_of(v, &num);

  assert_int(index, !=, E_NOT_FOUND);
  assert_size((size_t)index, ==, 0);

  num = 11;
  index = vector_index_of(v, &num);

  assert_int(index, ==, E_NOT_FOUND);

  return MUNIT_OK;
}

MunitResult test_vector_rindex_of(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int palindrome[9] = { 1, 2, 3, 4, 5, 4, 3, 2, 1 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 9; i++) {
    if(vector_push_front(v, &palindrome[i]) != E_OK)
      return MUNIT_ERROR;
  }

  int num = 3;
  int index = vector_rindex_of(v, &num);

  assert_int(index, !=, E_NOT_FOUND);
  assert_size((size_t)index, ==, 6);

  num = 1;
  index = vector_rindex_of(v, &num);

  assert_int(index, !=, E_NOT_FOUND);
  assert_size((size_t)index, ==, 8);

  num = -382;
  index = vector_rindex_of(v, &num);

  assert_int(index, ==, E_NOT_FOUND);

  return MUNIT_OK;
}

MunitResult test_vector_pop_back(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_ERROR;
  }

  for(size_t i=0; i < 10; i++) {
    int item=-1;

    if(IS_ERROR(vector_pop_back(v, &item)))
      return MUNIT_FAIL;

    assert_int(item, ==, int_data[10-i-1]);
  }

  assert_int(vector_pop_front(v, NULL), ==, E_FAIL);

  return MUNIT_OK;
}

MunitResult test_vector_pop_front(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_ERROR;
  }

  for(size_t i=0; i < 10; i++) {
    int item=-1;

    if(IS_ERROR(vector_pop_front(v, &item)))
      return MUNIT_FAIL;

    assert_int(item, ==, int_data[i]);
  }

  assert_int(vector_pop_front(v, NULL), ==, E_FAIL);

  return MUNIT_OK;
}

MunitResult test_vector_insert(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
  int int_data2[13] = { 1, -92, 2, 3, 4, 5, 67, 4, 3, 2, 1, 0, 4 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_ERROR;
  }

  int item = -92;

  if(IS_ERROR(vector_insert(v, 1, &item)))
      return MUNIT_FAIL;

  item = 67;

  if(IS_ERROR(vector_insert(v, 6, &item)))
      return MUNIT_FAIL;

  item = 4;

  if(IS_ERROR(vector_insert(v, 12, &item)))
      return MUNIT_FAIL;

  for(size_t i=0; i < 13; i++) {
    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  if(!IS_ERROR(vector_insert(v, 100, &item)))
    return MUNIT_FAIL;

  return MUNIT_OK;
}

MunitResult test_vector_remove_item(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
  int int_data2[9] = { 1, 2, 4, 5, 4, 3, 2, 1, 0 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_FAIL;
  }

  int item=3;

  if(IS_ERROR(vector_remove_item(v, &item)))
      return MUNIT_FAIL;

  assert_int(item, ==, 3);

  for(size_t i=0; i < 9; i++) {
    item=-1;

    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  item = 33;

  assert_int(vector_remove_item(v, &item), ==, E_NOT_FOUND);

  for(size_t i=0; i < 9; i++) {
    item=-1;

    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  return MUNIT_OK;
}

MunitResult test_vector_rremove_item(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
  int int_data2[9] = { 1, 2, 3, 4, 5, 4, 2, 1, 0 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_ERROR;
  }

  int item=3;

  if(IS_ERROR(vector_rremove_item(v, &item)))
      return MUNIT_FAIL;

  assert_int(item, ==, 3);

  for(size_t i=0; i < 9; i++) {
    item=-1;

    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  item = -6823;

  assert_int(vector_remove_item(v, &item), ==, E_NOT_FOUND);

  for(size_t i=0; i < 9; i++) {
    item=-1;

    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  return MUNIT_OK;
}

MunitResult test_vector_remove(const MunitParameter params[], void *data) {
  vector_t *v = (vector_t *)data;
  int int_data[10] = { 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
  int int_data2[7] = { 1, 2, 3, 5, 4, 1, 0 };

  if(v == NULL)
    return MUNIT_ERROR;

  for(size_t i=0; i < 10; i++) {
    if(vector_push_back(v, &int_data[i]) != E_OK)
      return MUNIT_ERROR;
  }

  int item = -1;

  if(IS_ERROR(vector_remove(v, 3, &item)))
      return MUNIT_FAIL;

  assert_int(item, ==, 4);

  if(IS_ERROR(vector_remove(v, 5, &item)))
      return MUNIT_FAIL;

  assert_int(item, ==, 3);

  if(IS_ERROR(vector_remove(v, 5, NULL)))
      return MUNIT_FAIL;

  for(size_t i=0; i < 7; i++) {
    assert_int(*(int *)VECTOR_ITEM(v, i), ==, int_data2[i]);
  }

  if(!IS_ERROR(vector_remove(v, 200, NULL)))
    return MUNIT_FAIL;

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
