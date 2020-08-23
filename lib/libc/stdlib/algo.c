#include <stdlib.h>

static void __gen_mt_numbers(void);

unsigned int __mt_array[624];
int __mt_index;

int abs(int num)
{
  return (num < 0 ? -num : num);
}

long int labs(long int num)
{
  return (num < 0 ? -num : num);
}

div_t div(int num, int denom)
{
  div_t result;

  result.quot = num / denom;
  result.rem = num % denom;

  return result;
}

ldiv_t ldiv(long num, long denom)
{
  ldiv_t result;

  result.quot = num / denom;
  result.rem = num % denom;

  return result;
}

void* bsearch(const void* key, const void* base, size_t n_elem, size_t size, int (*cmp)(const void* keyval, const void* datum))
{
  int cmp_result;
  void *middle_key = (void *)((unsigned)base + (n_elem / 2) * size);

  if(n_elem == 0)
    return NULL;

  cmp_result = cmp(middle_key, key);

  if( cmp_result == 0 )
    return middle_key;
  else
    return bsearch(key, cmp_result < 0 ? base : middle_key, n_elem / 2, size, cmp);
}

void __gen_mt_numbers(void)
{
  int i;
  unsigned int x;

  for(i=0; i <= 623; i++)
  {
    x = (0x80000000UL & __mt_array[i]) + (0x7FFFFFFFUL & __mt_array[(i+1) % 623]);
    __mt_array[i] = (__mt_array[(i+397)%624]) ^ (x >> 1);

    if( x % 2 == 1 )
      __mt_array[i] ^= 2567483615UL;
  }
}

void srand(unsigned int seed)
{
  int i;
  __mt_array[0] = seed;

  for(i=1; i <= 623; i++)
  {
    __mt_array[i] = 0xFFFFFFFF & (1812433253 * (__mt_array[i-1] ^
                                 (__mt_array[i-1] >> 30)) + i);
  }
}

int rand(void)
{
  unsigned int x;

  if( __mt_index == 0 )
    __gen_mt_numbers();

  x = __mt_array[__mt_index];
  x ^= x >> 11;
  x ^= (x << 7) & 2636928640UL;
  x ^= (x << 15) & 4022730752UL;
  x ^= x >> 18;

  __mt_index = (__mt_index+1) % 624;
  return x;
}
