#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/buddy.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

static void *test_setup_buddy(const MunitParameter params[],
    void *data);
static void test_teardown_buddy(void *fixture);

static MunitResult test_buddy_init(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_alloc_block(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_free_block(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_block_order(const MunitParameter params[], void *data);

MunitTest tests[] = {
  {
    "::init()",
    test_buddy_init,
    test_setup_buddy,
    test_teardown_buddy,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::block_order()",
    test_buddy_block_order,
    test_setup_buddy,
    test_teardown_buddy,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::alloc_block()",
    test_buddy_alloc_block,
    test_setup_buddy,
    test_teardown_buddy,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  {
    "::free_block()",
    test_buddy_free_block,
    test_setup_buddy,
    test_teardown_buddy,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite suite = {
  "kernel/buddy.c",
  tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

void *test_setup_buddy(const MunitParameter params[],
    void *data) {
  struct buddy_allocator *b = malloc(sizeof *b);

  if(!b)
    return NULL;

  void *region = malloc(BUDDIES_SIZE(5));

  if(!region) {
    free(b);
    return NULL;
  }

  if(IS_ERROR(buddy_init(b, 0x100000, 65536, region))) {
    free(region);
    free(b);
    return NULL;
  }

  return b;
}

void test_teardown_buddy(void *fixture) {
  if(fixture) {
    struct buddy_allocator *b = (struct buddy_allocator *)fixture;
    free(b->free_blocks[0]);
    free(b);
  }
}

MunitResult test_buddy_init(const MunitParameter params[],
    void *data) {
  struct buddy_allocator *b = (struct buddy_allocator *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  assert_size(buddy_free_bytes(b), ==, 0x100000);
  assert_false(buddy_coalesce_blocks(b));

  return MUNIT_OK;
}

MunitResult test_buddy_block_order(const MunitParameter params[], void *data) {
  struct buddy_allocator *b = (struct buddy_allocator *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  assert_size(buddy_block_order(b, 0), ==, 0);
  assert_size(buddy_block_order(b, 35000), ==, 0);
  assert_size(buddy_block_order(b, 100000), ==, 1);
  assert_size(buddy_block_order(b, 500000), ==, 3);
  assert_size(buddy_block_order(b, 1048576), ==, 4);

  return MUNIT_OK;
}

MunitResult test_buddy_alloc_block(const MunitParameter params[], void *data) {
  struct buddy_allocator *b = (struct buddy_allocator *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  struct memory_block b1, b2, b3, b4;

  assert_int(buddy_alloc_block(b, 34000, &b1), ==, E_OK);
  assert_int(buddy_alloc_block(b, 66000, &b2), ==, E_OK);
  assert_int(buddy_alloc_block(b, 35000, &b3), ==, E_OK);
  assert_int(buddy_alloc_block(b, 67000, &b4), ==, E_OK);

  assert_ptr_equal((char *)b1.ptr, (char *)(0));
  assert_ptr_equal((char *)b2.ptr, (char *)(128*1024));
  assert_ptr_equal((char *)b3.ptr, (char *)(64*1024));
  assert_ptr_equal((char *)b4.ptr, (char *)(256*1024));

  assert_size(b1.size, ==, 64*1024);
  assert_size(b2.size, ==, 128*1024);
  assert_size(b3.size, ==, 64*1024);
  assert_size(b4.size, ==, 128*1024);

  assert(buddy_free_bytes(b) == 0xA0000);

  return MUNIT_OK;
}

MunitResult test_buddy_free_block(const MunitParameter params[], void *data) {
  struct buddy_allocator *b = (struct buddy_allocator *)data;

  if(b == NULL)
    return MUNIT_ERROR;

  struct memory_block b1, b2, b3, b4;

  assert_int(buddy_alloc_block(b, 34000, &b1), ==, E_OK);
  assert_int(buddy_alloc_block(b, 66000, &b2), ==, E_OK);
  assert_int(buddy_alloc_block(b, 35000, &b3), ==, E_OK);
  assert_int(buddy_alloc_block(b, 67000, &b4), ==, E_OK);

  assert_int(buddy_free_block(b, &b2), ==, E_OK);
  assert_int(buddy_free_block(b, &b4), ==, E_OK);
  assert_int(buddy_free_block(b, &b1), ==, E_OK);
  assert_int(buddy_free_block(b, &b3), ==, E_OK);

  assert(buddy_free_bytes(b) == 0x100000);

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
