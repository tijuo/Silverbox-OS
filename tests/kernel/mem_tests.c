#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/memory.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

static void *test_setup_mem(const MunitParameter params[],
    void *data);
static void test_teardown_mem(void *fixture);

static MunitResult test_mem_kmemcpy(const MunitParameter params[],
    void *data);
static MunitResult test_mem_kmemset(const MunitParameter params[],
    void *data);
static MunitResult test_mem_kstrcpy(const MunitParameter params[],
    void *data);
static MunitResult test_mem_kstrncpy(const MunitParameter params[],
    void *data);
static MunitResult test_mem_kstrlen(const MunitParameter params[],
    void *data);

MunitTest tests[] = {
  {
    "::kmemcpy()",
    test_mem_kmemcpy,
    test_setup_mem,
    test_teardown_mem,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::kmemset()",
    test_mem_kmemset,
    test_setup_mem,
    test_teardown_mem,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::kstrcpy()",
    test_mem_kstrcpy,
    test_setup_mem,
    test_teardown_mem,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::kstrncpy()",
    test_mem_kstrncpy,
    test_setup_mem,
    test_teardown_mem,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::kstrlen()",
    test_mem_kstrlen,
    test_setup_mem,
    test_teardown_mem,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
  "kernel/mem.asm",
  tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

#define BUFSIZ 128

void *test_setup_mem(const MunitParameter params[],
    void *data) {
  return malloc(BUFSIZ);
}

void test_teardown_mem(void *fixture) {
  if(fixture) {
    free(fixture);
  }
}

static MunitResult test_mem_kmemcpy(const MunitParameter params[],
    void *data) {
  if(data == NULL)
    return MUNIT_ERROR;

  char *text1 = "Lorem ipsum dolor sit amet...";
  char *text2 = "Hello, world!";

  kmemcpy(data, text1, 30);
  assert_int(memcmp(data, text1, 30), ==, 0);
  assert_int(memcmp(text1, "Lorem ipsum dolor sit amet...", 30), ==, 0);

  kmemcpy(data, text2, 14);
  assert_int(memcmp(data, text2, 14), ==, 0);
  assert_int(memcmp(text2, "Hello, world!", 14), ==, 0);

  kmemcpy(data, text1, 0);
  assert_int(memcmp(data, text2, 14), ==, 0);
  assert_int(memcmp(text1, "Lorem ipsum dolor sit amet...", 14), ==, 0);

  return MUNIT_OK;
}

static MunitResult test_mem_kmemset(const MunitParameter params[],
    void *data) {
  if(data == NULL)
    return MUNIT_ERROR;

  char cs[8] = {0, 158, 222, -119, 87, 52, -28, -128};

  for(size_t i=0; i < 8; i++) {
    char c = cs[i];

    kmemset(data, c, BUFSIZ);

    for(size_t j=0; j < BUFSIZ; j++)
      assert_char(*(char *)ARR_ITEM(data, j, sizeof(char)), ==, c);

    assert_char(c, ==, cs[i]);
  }

  return MUNIT_OK;
}

static MunitResult test_mem_kstrcpy(const MunitParameter params[],
    void *data) {
  char *text1 = "The quick brown fox jumped over the lazy dog.";
  char *text2 = "How now, brown cow?";

  kstrcpy(data, text1);
  assert_int(strcmp(data, text1), ==, 0);
  assert_int(strcmp(text1, "The quick brown fox jumped over the lazy dog."), ==, 0);

  kstrcpy(data, text2);
  assert_int(strncmp(data, text2, 14), ==, 0);
  assert_int(strncmp(text2, "How now, brown cow?", 14), ==, 0);

  return MUNIT_OK;
}

static MunitResult test_mem_kstrncpy(const MunitParameter params[],
    void *data) {

  char *text1 = "The quick brown fox jumped over the lazy dog.";
  char *text2 = "How now, brown cow?";

  strcpy((char *)data, text1);
  kstrncpy((char *)ARR_ITEM(data, 16, sizeof(char)), &text2[15], 3*sizeof(char));
  kstrncpy((char *)ARR_ITEM(data, 41, sizeof(char)), &text1[16], 3*sizeof(char));

  assert_int(strcmp(data, "The quick brown cow jumped over the lazy fox."), ==, 0);

  kstrncpy(data, &text2[8], 12*sizeof(char));

  assert_int(strcmp(data, " brown cow?"), ==, 0);

  kstrncpy(data, text1, 0);
  assert_int(strcmp(data, " brown cow?"), ==, 0);
  assert_int(strcmp(text1, "The quick brown fox jumped over the lazy dog."), ==, 0);

  char buf[10];

  memset(buf, 'a', 10 * sizeof(char));

  kstrncpy(buf, "testtest", 10 * sizeof(char));

  assert_int(memcmp(buf, "testtest\0\0", 10 * sizeof(char)), ==, 0);

  return MUNIT_OK;
}

static MunitResult test_mem_kstrlen(const MunitParameter params[],
    void *data) {

  char *text1 = "This string contains 35 characters.";
  char *text2 = "This one has 16.";
  char *text3 = "";
  char *text4 = "This one has 16.\0\0Actually, it contains 43.";

  assert_size(kstrlen(text1), ==, 35);
  assert_size(kstrlen(text2), ==, 16);
  assert_size(kstrlen(text3), ==, 0);
  assert_size(kstrlen(text4), ==, 16);

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
