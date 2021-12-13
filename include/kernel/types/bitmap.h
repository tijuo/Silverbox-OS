#ifndef KERNEL_TYPES_BITMAP_H
#define KERNEL_TYPES_BITMAP_H

#include <stddef.h>
#include <util.h>

/**
  A bitmap is a compact array of bits.
*/

typedef struct bitmap {
  uint64_t *data;

  /// Number of bits
  size_t count;
} bitmap_t;

/**
  Calculate number of bytes needed to store a bitmap
  according to the number of bits in the bitmap. This
  only calculates the size of the bit array itself, not
  including the bitmap struct.
  @param bits The number of bits in the bitmap.
  @return The bitmap size in bytes.
*/

#define BITMAP_BYTES(bits) (ALIGN(bits, 8 * sizeof(uint64_t)) / 8)

/**
  Calculate the number of array elements needed to represent a bitmap.
  This only calculates the size of the bit array itself, not
  including the bitmap struct.
  @param bits The number of bits in the bitmap.
  @return The number of array elements.
*/

#define BITMAP_ELEMS(bits) (ALIGN(bits, 8 * sizeof(uint64_t)) / sizeof(uint64_t))

/**
  Set the value of a bit to 1.

  @param bitmap The bitmap.
  @param bit The index of the bit.
*/

NON_NULL_PARAMS static inline void bitmap_set(const bitmap_t *bitmap, size_t bit) {
  bitmap->data[bit / 64] |= 1 << ((bit) & 0x3F);
}

/**
  Set the value of a bit to 0.

  @param bitmap The bitmap.
  @param bit The index of the bit.
*/

NON_NULL_PARAMS static inline void bitmap_clear(const bitmap_t *bitmap, size_t bit) {
  bitmap->data[bit / 64] &= ~(1 << ((bit) & 0x3F));
}

/**
  Toggle the value of a bit. If the bit is 1, it's cleared. If it's
  0, then it's set.

  @param bitmap The bitmap.
  @param bit The index of the bit.
*/

NON_NULL_PARAMS static inline void bitmap_toggle(const bitmap_t *bitmap, size_t bit) {
  bitmap->data[bit / 64] ^= 1 << ((bit) & 0x3F);
}

/**
  Test whether a bit is set.

  @param bitmap The bitmap.
  @param bit The index of the bit to test.
  @return true, if the value of the bit is 1. false, if it's 0.
*/

NON_NULL_PARAMS PURE static inline bool bitmap_is_set(const bitmap_t *bitmap, size_t bit) {
  return !(bitmap->data[bit / 64] & 1 << ((bit) & 0x3F));
}

/**
  Test whether a bit is clear.

  @param bitmap The bitmap.
  @param bit The index of the bit to test.
  @return true, if the value of the bit is 0. false, if it's 1.
*/

NON_NULL_PARAMS PURE static inline bool bitmap_is_clear(const bitmap_t *bitmap, size_t bit) {
  return !(bitmap_is_set(bitmap, bit);
}

/**
  Initialize a bitmap.

  @param bitmap The bitmap structure.
  @param elements The number of bits to be used for the bitmap.
  @param set_bits If true, then the bitmap bits are all initialized to 1. Otherwise,
  the bits are initialized to 0.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAMS int bitmap_init(struct bitmap *bitmap, size_t elements, bool set_bits);

/**
  Initialize a bitmap at a specified memory region.

  @param bitmap The bitmap structure.
  @param elements The number of bits to be used for the bitmap.
  @param mem_region The buffer to be used for the bitmap. It must be large enough to
  store the bits.
  @param set_bits If true, then the bitmap bits are all initialized to 1. Otherwise,
  the bits are initialized to 0.
  @return E_OK, on success.
*/

NON_NULL_PARAMS int bitmap_init_at(struct bitmap *restrict bitmap, size_t elements,
  void *restrict mem_region, bool set_bits);

/**
  Release any allocated memory for the bitmap. Once this function is called, none of
  non bitmap_init*() functions should be called on the bitmap. This function shouldn't be
  called if the bitmap was initialized with bitmap_init_at().

  @param bitmap The bitmap.
  @return E_OK, on success.
*/

NON_NULL_PARAMS int bitmap_release(struct bitmap *bitmap);

/**
  Search for the last occurrence of a set or cleared bit.

  @param bitmap The bitmap.
  @param set true, if searching for a 1 bit. false, if searching for a 0 bit.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/

NON_NULL_PARAMS PURE long int bitmap_rfind(const bitmap_t *bitmap, bool set);

/**
  Search for the first occurrence of a set or cleared bit.

  @param bitmap The bitmap.
  @param set true, if searching for a 1 bit. false, if searching for a 0 bit.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/

NON_NULL_PARAMS PURE long int bitmap_find(const bitmap_t *bitmap, bool set);

/**
  Find the first occurrence of a set bit.

  @param bitmap The bitmap.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/

NON_NULL_PARAMS PURE static long int bitmap_find_first_set(const bitmap_t *bitmap) {
  return bitmap_find(bitmap, true);
}

/**
  Find the first occurrence of a set cleared.

  @param bitmap The bitmap.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/


NON_NULL_PARAMS PURE static long int bitmap_find_first_cleared(const bitmap_t *bitmap) {
  return bitmap_find(bitmap, false);
}

/**
  Find the last occurrence of a set bit.

  @param bitmap The bitmap.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/

NON_NULL_PARAMS PURE static long int bitmap_find_last_set(const bitmap_t *bitmap) {
  return bitmap_rfind(bitmap, true);
}

/**
  Find the last occurrence of a set cleared.

  @param bitmap The bitmap.
  @return The index of the found bit. E_NOT_FOUND if the bit couldn't be located.
*/


NON_NULL_PARAMS PURE static long int bitmap_find_last_cleared(const bitmap_t *bitmap) {
  return bitmap_rfind(bitmap, false);
}

#endif /* KERNEL_TYPES_BITMAP_H */
