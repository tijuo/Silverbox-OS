#include <smmintrin.h>
#include <immintrin.h>

int main(void)
{
  __m128i values = _mm_setr_epi32(0xC0FFEE, 0x1BADB002, 0xCAFEBABE, 0xC001D00D);
  __m128i values2 = _mm_setr_epi32(1,2,3,4);
  __m128i values3 = _mm_xor_si128(values2, values2);

  return 0;
}
