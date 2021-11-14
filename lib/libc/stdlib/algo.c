#include <stdlib.h>

static void gen_mt_numbers(void);

static unsigned int mt_array[624];
static int mt_index;

int abs(int num) {
  return (num < 0 ? -num : num);
}

long int labs(long int num) {
  return (num < 0 ? -num : num);
}

div_t div(int num, int denom) {
  div_t result;

  result.quot = num / denom;
  result.rem = num % denom;

  return result;
}

ldiv_t ldiv(long num, long denom) {
  ldiv_t result;

  result.quot = num / denom;
  result.rem = num % denom;

  return result;
}

void* bsearch(const void *key, const void *base, size_t n_elem, size_t size,
              int (*cmp)(const void *keyval, const void *datum))
{
  int cmp_result;
  void *middle_key = (void*)((unsigned)base + (n_elem / 2) * size);

  if(n_elem == 0)
    return NULL;

  cmp_result = cmp(middle_key, key);

  if(cmp_result == 0)
    return middle_key;
  else
    return bsearch(key, cmp_result < 0 ? base : middle_key, n_elem / 2, size,
                   cmp);
}

void gen_mt_numbers(void) {
  int i;
  unsigned int x;

  for(i = 0; i <= 623; i++) {
    x = (0x80000000UL & mt_array[i]) + (0x7FFFFFFFUL & mt_array[(i + 1) % 623]);
    mt_array[i] = (mt_array[(i + 397) % 624]) ^ (x >> 1);

    if(x % 2 == 1)
      mt_array[i] ^= 2567483615UL;
  }
}

void srand(unsigned int seed) {
  int i;
  mt_array[0] = seed;

  for(i = 1; i <= 623; i++) {
    mt_array[i] = 0xFFFFFFFF
        & (1812433253 * (mt_array[i - 1] ^ (mt_array[i - 1] >> 30)) + i);
  }
}

int rand(void) {
  unsigned int x;

  if(mt_index == 0)
    gen_mt_numbers();

  x = mt_array[mt_index];
  x ^= x >> 11;
  x ^= (x << 7) & 2636928640UL;
  x ^= (x << 15) & 4022730752UL;
  x ^= x >> 18;

  mt_index = (mt_index + 1) % 624;
  return x;
}
