#define MUNIT_ENABLE_ASSERT_ALIASES
#include <munit.h>
#include <kernel/buddy.h>
#include <kernel/error.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_SIZE	1048576
#define BLOCK_SIZE	65536

static void *test_setup_buddy(const MunitParameter params[],
    void *data);
static void test_teardown_buddy(void *fixture);

static MunitResult test_buddy_init(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_alloc_block(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_free_block(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_block_order(const MunitParameter params[],
    void *data);
static MunitResult test_buddy_reserve_block(const MunitParameter params[],
    void *data);

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
  {
    "::reserve_block()",
    test_buddy_reserve_block,
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

struct buddy_fixture {
  struct buddy_allocator *allocator;
  void *bitmap;
  void *memory;
  void *mem_start;
};

void *test_setup_buddy(const MunitParameter params[],
    void *data) {
  struct buddy_fixture *fixture = munit_malloc(sizeof *fixture);

  fixture->allocator = munit_malloc(sizeof *fixture->allocator);

  fixture->bitmap = munit_malloc(MEMORY_SIZE / (BLOCK_SIZE * 8)
                      + (MEMORY_SIZE % (BLOCK_SIZE * 8) ? 1 : 0));

  fixture->memory = munit_malloc(2*MEMORY_SIZE);
  fixture->mem_start = (void *)((uintptr_t)fixture->memory
    + MEMORY_SIZE - ((uintptr_t)fixture->memory & (MEMORY_SIZE-1)));

  if(IS_ERROR(buddy_init(fixture->allocator, MEMORY_SIZE, BLOCK_SIZE,
                         fixture->bitmap, fixture->mem_start)))
    goto init_fail;

  return fixture;

init_fail:
  free(fixture->memory);
  free(fixture->bitmap);
  free(fixture->allocator);
  free(fixture);
  return NULL;
}

void test_teardown_buddy(void *data) {
  struct buddy_fixture *fixture = (struct buddy_fixture *)data;

  if(fixture) {
    free(fixture->memory);
    free(fixture->bitmap);
    free(fixture->allocator);
    free(fixture);
  }
}

MunitResult test_buddy_init(const MunitParameter params[],
    void *data) {
  struct buddy_allocator *b = ((struct buddy_fixture *)data)->allocator;

  if(b == NULL)
    return MUNIT_ERROR;

  assert_size(buddy_free_bytes(b), ==, 0x100000);

  return MUNIT_OK;
}

MunitResult test_buddy_block_order(const MunitParameter params[], void *data) {
  struct buddy_allocator *b = ((struct buddy_fixture *)data)->allocator;

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
  struct buddy_fixture *fixture = (struct buddy_fixture *)data;
  struct buddy_allocator *b = fixture->allocator;

  if(b == NULL)
    return MUNIT_ERROR;

  struct memory_block b1, b2, b3, b4;

  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE);

  assert_int(buddy_alloc_block(b, 34000, &b1), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE-BLOCK_SIZE);

  assert_int(buddy_alloc_block(b, 66000, &b2), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE-3*BLOCK_SIZE);

  assert_int(buddy_alloc_block(b, 35000, &b3), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE-4*BLOCK_SIZE);

  assert_int(buddy_alloc_block(b, 67000, &b4), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE-6*BLOCK_SIZE);

  assert_ptr_equal((char *)b1.ptr, (char *)((uintptr_t)fixture->mem_start));
  assert_ptr_equal((char *)b2.ptr, (char *)((uintptr_t)fixture->mem_start + 2*BLOCK_SIZE));
  assert_ptr_equal((char *)b3.ptr, (char *)((uintptr_t)fixture->mem_start + BLOCK_SIZE));
  assert_ptr_equal((char *)b4.ptr, (char *)((uintptr_t)fixture->mem_start + 4*BLOCK_SIZE));

  assert_size(b1.size, ==, BLOCK_SIZE);
  assert_size(b2.size, ==, 2*BLOCK_SIZE);
  assert_size(b3.size, ==, BLOCK_SIZE);
  assert_size(b4.size, ==, 2*BLOCK_SIZE);

  return MUNIT_OK;
}

MunitResult test_buddy_free_block(const MunitParameter params[], void *data) {
  struct buddy_allocator *b = ((struct buddy_fixture *)data)->allocator;

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

  assert_size(buddy_free_bytes(b), ==, 0x100000);

  return MUNIT_OK;
}

MunitResult test_buddy_reserve_block(const MunitParameter params[], void *data) {
  struct buddy_fixture *fixture = (struct buddy_fixture *)data;
  struct buddy_allocator *b = fixture->allocator;

  if(b == NULL)
    return MUNIT_ERROR;

  struct memory_block b1 = { .ptr=NULL, .size=0 };
  struct memory_block b2 = { .ptr=NULL, .size=0 };
  struct memory_block b3 = { .ptr=NULL, .size=0 };
  struct memory_block b4 = { .ptr=NULL, .size=0 };

  assert_int(buddy_reserve_block(b,
    (void *)((uintptr_t)fixture->mem_start + BLOCK_SIZE),
    2 * BLOCK_SIZE, &b1), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE - 2*BLOCK_SIZE);
  assert_ptr_equal(b1.ptr, (void *)((uintptr_t)fixture->mem_start + BLOCK_SIZE));
  assert_size(b1.size, ==, 2*BLOCK_SIZE);

  assert_int(buddy_reserve_block(b,
    (void *)((uintptr_t)fixture->mem_start + 8*BLOCK_SIZE),
    4 * BLOCK_SIZE, &b2), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE - 6*BLOCK_SIZE);
  assert_ptr_equal(b2.ptr, (void *)((uintptr_t)fixture->mem_start + 8*BLOCK_SIZE));
  assert_size(b2.size, ==, 4*BLOCK_SIZE);

  assert_int(buddy_reserve_block(b,
    (void *)((uintptr_t)fixture->mem_start + 15*BLOCK_SIZE),
    BLOCK_SIZE, &b3), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE - 7*BLOCK_SIZE);
  assert_ptr_equal(b3.ptr, (void *)((uintptr_t)fixture->mem_start + 15*BLOCK_SIZE));
  assert_size(b3.size, ==, BLOCK_SIZE);

  assert_int(buddy_reserve_block(b,
    (void *)((uintptr_t)fixture->mem_start + 3*BLOCK_SIZE),
    BLOCK_SIZE, &b4), ==, E_OK);
  assert_size(buddy_free_bytes(b), ==, MEMORY_SIZE - 8*BLOCK_SIZE);
  assert_ptr_equal(b4.ptr, (void *)((uintptr_t)fixture->mem_start + 3*BLOCK_SIZE));
  assert_size(b4.size, ==, BLOCK_SIZE);

  return MUNIT_OK;
}

int main(int argc, char *argv[]) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
