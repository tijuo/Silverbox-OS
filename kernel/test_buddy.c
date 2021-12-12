#include <stdio.h>
#include "buddy.h"
#include <assert.h>
#include <stdlib.h>

int main(void) {
  struct buddies b;
  size_t orders = 5;
  assert(BUDDIES_SIZE(orders) == (orders*sizeof(uint32_t)+orders*sizeof(uint32_t *)+((1 << (orders+1)) - 1)*sizeof(uint32_t)));

  char *addr = malloc(BUDDIES_SIZE(orders));

  assert(addr != NULL);

  assert(buddies_init(&b, 0x100000, addr) == E_OK);
  assert(buddies_block_order(&b, 37000) == 0);
  assert(buddies_block_order(&b, 512) == 0);

  assert(buddies_free_bytes(&b) == 0x100000);

  struct memory_block b1, b2, b3, b4;

  assert(buddies_alloc_block(&b, 34000, &b1) == E_OK);
  assert(buddies_alloc_block(&b, 66000, &b2) == E_OK);
  assert(buddies_alloc_block(&b, 35000, &b3) == E_OK);
  assert(buddies_alloc_block(&b, 67000, &b4) == E_OK);

  printf("block 1 ptr: %p size: %u\n", b1.ptr, b1.size);
  printf("block 2 ptr: %p size: %u\n", b2.ptr, b2.size);
  printf("block 3 ptr: %p size: %u\n", b3.ptr, b3.size);
  printf("block 4 ptr: %p size: %u\n", b4.ptr, b4.size);

  assert((char *)b1.ptr == (char *)(0));
  assert((char *)b2.ptr == (char *)(128*1024));
  assert((char *)b3.ptr == (char *)(64*1024));
  assert((char *)b4.ptr == (char *)(256*1024));

  assert(b1.size == 64*1024);
  assert(b2.size == 128*1024);
  assert(b3.size == 64*1024);
  assert(b4.size == 128*1024);

  assert(buddies_free_bytes(&b) == 0xA0000);

  assert(buddies_free_block(&b, &b2) == E_OK);
  assert(buddies_free_block(&b, &b4) == E_OK);
  assert(buddies_free_block(&b, &b1) == E_OK);
  assert(buddies_free_block(&b, &b3) == E_OK);

  assert(buddies_free_bytes(&b) == 0x100000);

  free(addr);

  return EXIT_SUCCESS;
}
